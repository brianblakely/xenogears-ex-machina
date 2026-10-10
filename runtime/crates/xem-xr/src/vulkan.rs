//! The wgpu device of an OpenXR system, created through XR_KHR_vulkan_enable2.
//!
//! The runtime chooses the physical device and adds the instance and device
//! extensions it needs to the create infos wgpu-hal would use itself; the
//! resulting Vulkan handles are wrapped into wgpu-hal and then wgpu, which owns
//! (and finally destroys) them like devices it opened on its own.

use crate::XrError;
use ash::vk::{self, Handle};
use openxr as xr;
use wgpu::hal::{api::Vulkan as VulkanApi, vulkan as hal};
use xem_render::Gpu;

/// The raw Vulkan objects a session is bound to (`XrGraphicsBindingVulkan2KHR`).
#[derive(Clone, Copy, Debug)]
pub struct VulkanBinding {
    pub instance: vk::Instance,
    pub physical_device: vk::PhysicalDevice,
    pub device: vk::Device,
    pub queue_family_index: u32,
    pub queue_index: u32,
}

/// The highest Vulkan version wgpu-hal 30 targets.
const WGPU_VULKAN_VERSION: u32 = vk::API_VERSION_1_3;

fn vk_error(context: &'static str) -> impl FnOnce(vk::Result) -> XrError {
    move |result| XrError::new(context, format!("{result:?}"))
}

fn xr_version(version: u32) -> xr::Version {
    xr::Version::new(
        vk::api_version_major(version) as u16,
        vk::api_version_minor(version) as u16,
        0,
    )
}

/// Creates the Vulkan instance and device for `system` through the runtime and
/// wraps them as one wgpu [`Gpu`] with a single queue, which the session will
/// share with the runtime.
pub(crate) fn create_gpu(
    instance: &xr::Instance,
    system: xr::SystemId,
) -> Result<(Gpu, VulkanBinding), XrError> {
    let requirements = instance
        .graphics_requirements::<xr::Vulkan>(system)
        .map_err(XrError::xr("xrGetVulkanGraphicsRequirements2KHR"))?;
    // SAFETY: loads the system's Vulkan loader, as wgpu-hal does.
    let entry = unsafe { ash::Entry::load() }
        .map_err(|error| XrError::new("loading Vulkan", error.to_string()))?;
    let loader_version = unsafe { entry.try_enumerate_instance_version() }
        .map_err(vk_error("vkEnumerateInstanceVersion"))?
        .unwrap_or(vk::API_VERSION_1_0);
    let api_version = loader_version.min(WGPU_VULKAN_VERSION);
    if xr_version(api_version) < requirements.min_api_version_supported {
        return Err(XrError::new(
            "Vulkan version",
            format!(
                "the runtime needs Vulkan {} or later, the loader offers {}",
                requirements.min_api_version_supported,
                xr_version(loader_version)
            ),
        ));
    }

    let flags = wgpu::InstanceFlags::from_build_config().with_env();
    let extensions = hal::Instance::desired_extensions(&entry, api_version, flags)
        .map_err(|error| XrError::new("Vulkan instance extensions", error.to_string()))?;
    let extension_names: Vec<_> = extensions.iter().map(|name| name.as_ptr()).collect();
    let app_info = vk::ApplicationInfo::default()
        .application_name(c"Xenogears Ex Machina")
        .engine_name(c"wgpu-hal")
        .api_version(api_version);
    let instance_info = vk::InstanceCreateInfo::default()
        .application_info(&app_info)
        .enabled_extension_names(&extension_names);
    let get_instance_proc_addr = entry.static_fn().get_instance_proc_addr;
    // SAFETY: the create info and the entry point are valid for the call; the
    // runtime adds its extensions and calls vkCreateInstance.
    let raw_instance = unsafe {
        instance.create_vulkan_instance(
            system,
            std::mem::transmute::<
                vk::PFN_vkGetInstanceProcAddr,
                xr::sys::platform::VkGetInstanceProcAddr,
            >(get_instance_proc_addr),
            std::ptr::from_ref(&instance_info).cast(),
        )
    }
    .map_err(XrError::xr("xrCreateVulkanInstanceKHR"))?
    .map_err(|result| vk_error("vkCreateInstance via OpenXR")(vk::Result::from_raw(result)))?;
    // SAFETY: the runtime returned a live instance created from `entry`.
    let ash_instance = unsafe {
        ash::Instance::load(
            entry.static_fn(),
            vk::Instance::from_raw(raw_instance as u64),
        )
    };

    // wgpu-hal takes ownership of the instance (no drop callback).
    // SAFETY: the instance was created from `entry` with `api_version`, `flags`
    // and exactly `desired_extensions` (plus the runtime's).
    let hal_instance = unsafe {
        hal::Instance::from_raw(
            entry,
            ash_instance.clone(),
            api_version,
            android_sdk_version(),
            None,
            extensions,
            flags,
            Default::default(),
            false,
            None,
        )
    }
    .map_err(|error| XrError::new("wrapping the Vulkan instance", error.to_string()))?;

    // SAFETY: the instance came from xrCreateVulkanInstanceKHR for `system`.
    let physical_device = vk::PhysicalDevice::from_raw(
        unsafe { instance.vulkan_graphics_device(system, raw_instance) }
            .map_err(XrError::xr("xrGetVulkanGraphicsDevice2KHR"))? as u64,
    );
    let exposed = hal_instance
        .expose_adapter(physical_device)
        .ok_or_else(|| {
            XrError::new(
                "wgpu adapter",
                "the runtime's physical device does not meet wgpu's Vulkan requirements".into(),
            )
        })?;
    let queue_family_index =
        unsafe { ash_instance.get_physical_device_queue_family_properties(physical_device) }
            .iter()
            .position(|family| family.queue_flags.contains(vk::QueueFlags::GRAPHICS))
            .ok_or_else(|| XrError::new("Vulkan queue", "no graphics queue family".into()))?
            as u32;

    let features = wgpu::Features::empty();
    let limits = exposed.capabilities.limits.clone();
    let device_extensions = exposed.adapter.required_device_extensions(features);
    let mut device_features = exposed
        .adapter
        .physical_device_features(&device_extensions, features);
    let extension_names: Vec<_> = device_extensions.iter().map(|name| name.as_ptr()).collect();
    let queue_infos = [vk::DeviceQueueCreateInfo::default()
        .queue_family_index(queue_family_index)
        .queue_priorities(&[1.0])];
    let device_info = device_features.add_to_device_create(
        vk::DeviceCreateInfo::default()
            .queue_create_infos(&queue_infos)
            .enabled_extension_names(&extension_names),
    );
    // SAFETY: as for the instance; the runtime adds its device extensions.
    let raw_device = unsafe {
        instance.create_vulkan_device(
            system,
            std::mem::transmute::<
                vk::PFN_vkGetInstanceProcAddr,
                xr::sys::platform::VkGetInstanceProcAddr,
            >(get_instance_proc_addr),
            physical_device.as_raw() as _,
            std::ptr::from_ref(&device_info).cast(),
        )
    }
    .map_err(XrError::xr("xrCreateVulkanDeviceKHR"))?
    .map_err(|result| vk_error("vkCreateDevice via OpenXR")(vk::Result::from_raw(result)))?;
    // SAFETY: a live device of `physical_device`.
    let ash_device = unsafe {
        ash::Device::load(
            ash_instance.fp_v1_0(),
            vk::Device::from_raw(raw_device as u64),
        )
    };
    let binding = VulkanBinding {
        instance: ash_instance.handle(),
        physical_device,
        device: ash_device.handle(),
        queue_family_index,
        queue_index: 0,
    };

    // wgpu-hal takes ownership of the device (no drop callback).
    // SAFETY: created from this adapter with `device_extensions` and
    // `physical_device_features(device_extensions, features)`.
    let open = unsafe {
        exposed.adapter.device_from_raw(
            ash_device,
            None,
            &device_extensions,
            features,
            &limits,
            &wgpu::MemoryHints::Performance,
            queue_family_index,
            0,
        )
    }
    .map_err(|error| XrError::new("wrapping the Vulkan device", error.to_string()))?;
    // SAFETY: `hal_instance` is a valid Vulkan instance; the adapter and device
    // were created from it.
    let instance = unsafe { wgpu::Instance::from_hal::<VulkanApi>(hal_instance) };
    let adapter = unsafe { instance.create_adapter_from_hal(exposed) };
    let (device, queue) = unsafe {
        adapter.create_device_from_hal(
            open,
            &wgpu::DeviceDescriptor {
                label: Some("xem xr"),
                required_features: features,
                required_limits: limits,
                ..Default::default()
            },
        )
    }
    .map_err(|error| XrError::new("wgpu device", error.to_string()))?;
    let gpu = Gpu {
        instance,
        adapter,
        device,
        queue,
    };
    Ok((gpu, binding))
}

#[cfg(target_os = "android")]
fn android_sdk_version() -> u32 {
    // wgpu-hal only uses it to work around old drivers; Horizon OS is API 32+.
    32
}

#[cfg(not(target_os = "android"))]
fn android_sdk_version() -> u32 {
    0
}

/// The wgpu format of a Vulkan swapchain format the renderer can target.
pub(crate) fn wgpu_format(format: u32) -> Option<wgpu::TextureFormat> {
    use wgpu::TextureFormat as F;
    Some(match vk::Format::from_raw(format as i32) {
        vk::Format::R8G8B8A8_SRGB => F::Rgba8UnormSrgb,
        vk::Format::B8G8R8A8_SRGB => F::Bgra8UnormSrgb,
        vk::Format::R8G8B8A8_UNORM => F::Rgba8Unorm,
        vk::Format::B8G8R8A8_UNORM => F::Bgra8Unorm,
        _ => return None,
    })
}

/// Wraps a runtime-owned swapchain image as a wgpu texture. wgpu never
/// destroys the image or its memory; it starts in the layout the runtime
/// guarantees at acquire (`COLOR_ATTACHMENT_OPTIMAL`).
///
/// # Safety
///
/// `image` must be an image of a swapchain created on `device` with `desc`'s
/// size, format and (at least) usage, and must outlive the texture and every
/// view of it.
pub(crate) unsafe fn wrap_swapchain_image(
    device: &wgpu::Device,
    image: u64,
    desc: &wgpu::TextureDescriptor<'_>,
) -> wgpu::Texture {
    let hal_device = unsafe { device.as_hal::<VulkanApi>() }.expect("a Vulkan device");
    let hal_desc = wgpu::hal::TextureDescriptor {
        label: desc.label,
        size: desc.size,
        mip_level_count: desc.mip_level_count,
        sample_count: desc.sample_count,
        dimension: desc.dimension,
        format: desc.format,
        usage: wgpu::TextureUses::COLOR_TARGET,
        memory_flags: wgpu::hal::MemoryFlags::empty(),
        view_formats: Vec::new(),
    };
    let texture = unsafe {
        hal_device.texture_from_raw(
            vk::Image::from_raw(image),
            &hal_desc,
            Some(Box::new(|| {})),
            hal::TextureMemory::External,
        )
    };
    drop(hal_device);
    unsafe {
        device.create_texture_from_hal::<VulkanApi>(texture, desc, wgpu::TextureUses::COLOR_TARGET)
    }
}

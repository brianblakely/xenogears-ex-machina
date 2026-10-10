use std::ffi::{CStr, c_char, c_int, c_void};

use xem_core::memory::{GameMemory, OutOfBounds};
use xem_core::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};

include!(concat!(env!("OUT_DIR"), "/imports.rs"));

#[repr(C)]
struct Instance {
    _opaque: [u8; 0],
}

/// `struct w2c_xem` of the generated glue.
#[repr(C)]
struct Env {
    host: *mut c_void,
    import: extern "C" fn(*mut c_void, u32, *const u32, u32, *mut u32) -> c_int,
    instance: *mut Instance,
    resume_value: u32,
    area: u32,
}

unsafe extern "C" {
    fn xem_game_new(env: *mut Env) -> *mut Instance;
    fn xem_game_free(instance: *mut Instance);
    fn xem_game_run(instance: *mut Instance, kind: u32, arg: u32) -> c_int;
    fn xem_game_call(instance: *mut Instance, address: u32) -> c_int;
    fn xem_game_interrupt(instance: *mut Instance, irq: u32, detail: u32) -> c_int;
    fn xem_game_trap_description(code: c_int) -> *const c_char;
    fn xem_game_memory(instance: *mut Instance, size: *mut u64) -> *mut u8;
    fn xem_game_stack_pointer(instance: *mut Instance) -> *mut u32;
    fn xem_game_async_state(instance: *mut Instance) -> u32;
    fn xem_game_stop_unwind(instance: *mut Instance);
    fn xem_game_start_rewind(instance: *mut Instance);
}

/// The handler pointer the glue's `host` carries.
struct Host {
    handler: Option<*mut dyn ImportHandler>,
    memory: RawMemory,
}

#[derive(Clone, Copy)]
struct RawMemory {
    data: *mut u8,
    size: usize,
}

impl RawMemory {
    fn check(&self, address: u32, len: usize) -> Result<usize, OutOfBounds> {
        let start = address as usize;
        match start.checked_add(len) {
            Some(end) if end <= self.size => Ok(start),
            _ => Err(OutOfBounds { address, len }),
        }
    }
}

impl GameMemory for RawMemory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        let start = self.check(address, out.len())?;
        // SAFETY: in bounds of the module's memory, which never grows or moves.
        unsafe { std::ptr::copy_nonoverlapping(self.data.add(start), out.as_mut_ptr(), out.len()) };
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
        let start = self.check(address, data.len())?;
        // SAFETY: as above.
        unsafe { std::ptr::copy_nonoverlapping(data.as_ptr(), self.data.add(start), data.len()) };
        Ok(())
    }
}

extern "C" fn import(host: *mut c_void, index: u32, args: *const u32, count: u32, result: *mut u32) -> c_int {
    // SAFETY: `host` is the Box<Host> the module owns; `args` has `count` words.
    let host = unsafe { &mut *(host as *mut Host) };
    let args = unsafe { std::slice::from_raw_parts(args, count as usize) };
    let Some(handler) = host.handler else { return 2 };
    let name = IMPORT_NAMES[index as usize];
    // SAFETY: the runtime keeps the handler valid and unaliased during exports.
    let action = unsafe { (*handler).import(Import { name, args }, &mut host.memory) };
    match action {
        Action::Return(value) => {
            unsafe { *result = value };
            0
        }
        Action::Unwind => 1,
        Action::Trap => 2,
    }
}

pub struct NativeModule {
    instance: *mut Instance,
    env: Box<Env>,
    host: Box<Host>,
}

impl NativeModule {
    pub fn new() -> Self {
        let mut host = Box::new(Host { handler: None, memory: RawMemory { data: std::ptr::null_mut(), size: 0 } });
        let mut env = Box::new(Env {
            host: &mut *host as *mut Host as *mut c_void,
            import,
            instance: std::ptr::null_mut(),
            resume_value: 0,
            area: 0,
        });
        // SAFETY: env outlives the instance (both owned here, instance freed first).
        let instance = unsafe { xem_game_new(&mut *env) };
        let mut size = 0;
        let data = unsafe { xem_game_memory(instance, &mut size) };
        host.memory = RawMemory { data, size: size as usize };
        NativeModule { instance, env, host }
    }

    fn trap(code: c_int) -> Trap {
        // SAFETY: wasm2c returns a static string.
        let text = unsafe { CStr::from_ptr(xem_game_trap_description(code)) };
        Trap::Wasm(text.to_string_lossy().into_owned())
    }
}

impl Default for NativeModule {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for NativeModule {
    fn drop(&mut self) {
        unsafe { xem_game_free(self.instance) };
    }
}

impl GameModule for NativeModule {
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler) {
        self.host.handler = Some(handler);
    }

    fn set_resume_value(&mut self, value: u32) {
        self.env.resume_value = value;
    }

    fn run(&mut self, kind: u32, arg: u32) -> Result<(), Trap> {
        match unsafe { xem_game_run(self.instance, kind, arg) } {
            0 => Ok(()),
            code => Err(Self::trap(code)),
        }
    }

    fn call(&mut self, address: u32) -> Result<(), Trap> {
        match unsafe { xem_game_call(self.instance, address) } {
            0 => Ok(()),
            code => Err(Self::trap(code)),
        }
    }

    fn interrupt(&mut self, irq: u32, detail: u32) -> Result<(), Trap> {
        match unsafe { xem_game_interrupt(self.instance, irq, detail) } {
            0 => Ok(()),
            code => Err(Self::trap(code)),
        }
    }

    fn async_state(&mut self) -> AsyncState {
        AsyncState::from_raw(unsafe { xem_game_async_state(self.instance) })
    }

    fn stop_unwind(&mut self) {
        unsafe { xem_game_stop_unwind(self.instance) }
    }

    fn start_rewind(&mut self) {
        unsafe { xem_game_start_rewind(self.instance) }
    }

    fn stack_pointer(&mut self) -> u32 {
        unsafe { *xem_game_stack_pointer(self.instance) }
    }

    fn set_stack_pointer(&mut self, value: u32) {
        unsafe { *xem_game_stack_pointer(self.instance) = value }
    }

    fn memory(&mut self) -> &mut dyn GameMemory {
        &mut self.host.memory
    }

    fn globals(&mut self) -> Vec<u32> {
        vec![self.env.area]
    }

    fn set_globals(&mut self, values: &[u32]) {
        if let Some(&area) = values.first() {
            self.env.area = area;
        }
    }
}

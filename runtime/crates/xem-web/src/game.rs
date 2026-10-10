//! The game module in a browser: `game.wasm` instantiated by the page beside
//! this runtime module. The two modules keep separate memories and exchange
//! scalars; game memory is reached through copies from the game module's
//! `WebAssembly.Memory` (docs/runtime.md, The game module).
//!
//! The page builds the game's "xem" imports as one generic forward each,
//! `(...args) => xem_game_import(index, Uint32Array.of(...args))`, which
//! mirrors xem-game's native glue: while asyncify rewinds, the import stops
//! the rewind and returns the resume value; otherwise the runtime's handler
//! answers with a value, an unwind or a trap.

use std::cell::{Cell, RefCell};
use std::rc::Rc;

use js_sys::{Function, Reflect, Uint8Array, WebAssembly};
use wasm_bindgen::prelude::*;
use xem_core::memory::{GameMemory, OutOfBounds};
use xem_core::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};

/// Game memory through copies from the game module's memory.
#[derive(Clone)]
pub struct WebMemory {
    memory: WebAssembly::Memory,
}

impl WebMemory {
    fn view(&self, address: u32, len: usize) -> Result<Uint8Array, OutOfBounds> {
        let buffer = self.memory.buffer();
        let size = buffer.unchecked_ref::<js_sys::ArrayBuffer>().byte_length() as u64;
        if u64::from(address) + len as u64 > size {
            return Err(OutOfBounds { address, len });
        }
        Ok(Uint8Array::new_with_byte_offset_and_length(&buffer, address, len as u32))
    }
}

impl GameMemory for WebMemory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        self.view(address, out.len())?.copy_to(out);
        Ok(())
    }

    fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
        self.view(address, data.len())?.copy_from(data);
        Ok(())
    }
}

/// The game module's exports and what its imports need between calls.
struct Link {
    names: Vec<String>,
    memory: WebMemory,
    run: Function,
    call: Function,
    unwind_area: Function,
    start_unwind: Function,
    stop_unwind: Function,
    start_rewind: Function,
    stop_rewind: Function,
    get_state: Function,
    stack_pointer: WebAssembly::Global,
    heap_base: WebAssembly::Global,
    handler: Cell<Option<*mut dyn ImportHandler>>,
    resume_value: Cell<u32>,
    area: Cell<u32>,
}

thread_local! {
    /// The module the imports forward to (one game module per page).
    static LINK: RefCell<Option<Rc<Link>>> = const { RefCell::new(None) };
}

fn export(exports: &JsValue, name: &str) -> Result<JsValue, String> {
    let value = Reflect::get(exports, &JsValue::from_str(name)).map_err(|_| format!("no export {name}"))?;
    if value.is_undefined() {
        return Err(format!("the game module does not export {name}"));
    }
    Ok(value)
}

fn number(value: JsValue) -> u32 {
    value.as_f64().unwrap_or(0.0) as i64 as u32
}

/// A JS exception or wasm trap as text.
pub fn describe(error: &JsValue) -> String {
    if let Some(error) = error.dyn_ref::<js_sys::Error>() {
        return String::from(error.message());
    }
    error.as_string().unwrap_or_else(|| format!("{error:?}"))
}

impl Link {
    fn async_state(&self) -> AsyncState {
        AsyncState::from_raw(self.get_state.call0(&JsValue::NULL).map(number).unwrap_or(0))
    }
}

/// The game module as [`GameModule`].
pub struct WebModule {
    link: Rc<Link>,
    memory: WebMemory,
}

impl WebModule {
    /// Bind an instance of game.wasm whose "xem" imports, in `names` order,
    /// call [`xem_game_import`]. Replaces the module the imports reach.
    pub fn new(instance: &WebAssembly::Instance, names: Vec<String>) -> Result<Self, String> {
        let exports: JsValue = instance.exports().into();
        let function = |name: &str| export(&exports, name).map(|v| v.unchecked_into::<Function>());
        let global = |name: &str| export(&exports, name).map(|v| v.unchecked_into::<WebAssembly::Global>());
        let memory = WebMemory { memory: export(&exports, "memory")?.unchecked_into() };
        let link = Rc::new(Link {
            names,
            memory: memory.clone(),
            run: function("xem_run")?,
            call: function("xem_call")?,
            unwind_area: function("xem_unwind_area")?,
            start_unwind: function("asyncify_start_unwind")?,
            stop_unwind: function("asyncify_stop_unwind")?,
            start_rewind: function("asyncify_start_rewind")?,
            stop_rewind: function("asyncify_stop_rewind")?,
            get_state: function("asyncify_get_state")?,
            stack_pointer: global("__stack_pointer")?,
            heap_base: global("__heap_base")?,
            handler: Cell::new(None),
            resume_value: Cell::new(0),
            area: Cell::new(0),
        });
        LINK.set(Some(link.clone()));
        Ok(WebModule { link, memory })
    }

    pub fn memory_bytes(&self) -> u64 {
        self.memory.memory.buffer().unchecked_ref::<js_sys::ArrayBuffer>().byte_length() as u64
    }

    fn trap(error: JsValue) -> Trap {
        Trap::Wasm(describe(&error))
    }
}

impl Drop for WebModule {
    fn drop(&mut self) {
        LINK.with_borrow_mut(|link| {
            if link.as_ref().is_some_and(|l| Rc::ptr_eq(l, &self.link)) {
                *link = None;
            }
        });
    }
}

/// Every "xem" import of the game module: `index` into the import names.
#[wasm_bindgen]
pub fn xem_game_import(index: u32, args: &[u32]) -> Result<u32, JsValue> {
    let link = LINK.with_borrow(Clone::clone).ok_or_else(|| JsValue::from_str("no game module is bound"))?;
    if link.async_state() == AsyncState::Rewinding {
        link.stop_rewind.call0(&JsValue::NULL)?;
        return Ok(link.resume_value.get());
    }
    let handler = link.handler.get().ok_or_else(|| JsValue::from_str("no import handler"))?;
    let name = link.names.get(index as usize).ok_or_else(|| JsValue::from_str("unknown import"))?;
    let mut memory = link.memory.clone();
    // SAFETY: the runtime keeps the handler valid and unaliased while an
    // export runs, and imports only run inside exports.
    let action = unsafe { (*handler).import(Import { name, args }, &mut memory) };
    match action {
        Action::Return(value) => Ok(value),
        Action::Unwind => {
            let area = number(link.unwind_area.call0(&JsValue::NULL)?);
            link.area.set(area);
            link.start_unwind.call1(&JsValue::NULL, &area.into())?;
            Ok(0)
        }
        Action::Trap => Err(js_sys::Error::new(&format!("xem.{name} stopped the game")).into()),
    }
}

impl GameModule for WebModule {
    unsafe fn set_import_handler(&mut self, handler: *mut dyn ImportHandler) {
        self.link.handler.set(Some(handler));
    }

    fn set_resume_value(&mut self, value: u32) {
        self.link.resume_value.set(value);
    }

    fn run(&mut self, kind: u32, arg: u32) -> Result<(), Trap> {
        self.link.run.call2(&JsValue::NULL, &kind.into(), &arg.into()).map(drop).map_err(Self::trap)
    }

    fn call(&mut self, address: u32) -> Result<(), Trap> {
        self.link.call.call1(&JsValue::NULL, &address.into()).map(drop).map_err(Self::trap)
    }

    fn async_state(&mut self) -> AsyncState {
        self.link.async_state()
    }

    fn stop_unwind(&mut self) {
        let _ = self.link.stop_unwind.call0(&JsValue::NULL);
    }

    fn start_rewind(&mut self) {
        let _ = self.link.start_rewind.call1(&JsValue::NULL, &self.link.area.get().into());
    }

    fn stack_pointer(&mut self) -> u32 {
        number(self.link.stack_pointer.value())
    }

    fn set_stack_pointer(&mut self, value: u32) {
        self.link.stack_pointer.set_value(&JsValue::from(value as i32));
    }

    fn memory(&mut self) -> &mut dyn GameMemory {
        &mut self.memory
    }

    fn data_end(&mut self) -> u32 {
        number(self.link.heap_base.value())
    }

    fn globals(&mut self) -> Vec<u32> {
        vec![self.link.area.get()]
    }

    fn set_globals(&mut self, values: &[u32]) {
        if let Some(&area) = values.first() {
            self.link.area.set(area);
        }
    }
}

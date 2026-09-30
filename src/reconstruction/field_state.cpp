#include "xem/reconstruction/field_state.hpp"

namespace xem::reconstruction {

field::EventActor FieldActor::events() const { return field::original::read_event_actor(storage); }
field::ControlActor FieldActor::control() const {
    return field::original::read_control_actor(storage);
}

} // namespace xem::reconstruction

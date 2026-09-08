# Editor ownership

Reserved for the shared graphical editor shell and the level, cutscene, battle, and
minigame tools in Phases 11–12. Phase 0 implements none of those tools. Editors will
use the project's runtime/importers, deterministic state services, and Lua APIs;
their exports are native mod packages, never changes to the original asset store.

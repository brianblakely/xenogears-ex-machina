// The overlay functions by original entry address, for hosts that enter the
// overlay at an arbitrary function (menu_overlay.hpp, Overlay::call).
// Generated from the declarations in menu_overlay.hpp.
#include "xem/reconstruction/menu_overlay.hpp"

namespace xem::reconstruction::menu {

std::uint32_t Overlay::call(std::uint32_t address, const std::vector<std::uint32_t> &arguments) {
    const auto arg = [&](std::size_t i) {
        if (i >= arguments.size())
            throw MenuError("An overlay call lacks an argument word");
        return arguments[i];
    };
    static_cast<void>(arg);
    switch (address) {
    case 0x801c531cU:
        return run_command(arg(0));
    case 0x801c55a0U:
        field_menu_loop();
        return 0;
    case 0x801c58ecU:
        title_file_loop();
        return 0;
    case 0x801c5b54U:
        card_state_block(arg(0));
        return 0;
    case 0x801c5bb8U:
        party_block(arg(0));
        return 0;
    case 0x801c5c1cU:
        screen_image_block(arg(0));
        return 0;
    case 0x801c5c80U:
        block_354(arg(0));
        return 0;
    case 0x801c5ce4U:
        table_directory_block(arg(0));
        return 0;
    case 0x801c5d48U:
        block_340(arg(0));
        return 0;
    case 0x801c5dacU:
        block_344(arg(0));
        return 0;
    case 0x801c5e10U:
        primitive_block(arg(0));
        return 0;
    case 0x801c5e74U:
        field_blocks(arg(0));
        return 0;
    case 0x801c5f10U:
        allocate_blocks();
        return 0;
    case 0x801c5fe4U:
        leave_menu();
        return 0;
    case 0x801c62a8U:
        run_menu_mode();
        return 0;
    case 0x801c6400U:
        reset_card_state();
        return 0;
    case 0x801c65f4U:
        load_resources();
        return 0;
    case 0x801c6aa0U:
        set_up_party();
        return 0;
    case 0x801c6d4cU:
        reset_buffer_index();
        return 0;
    case 0x801c6d5cU:
        reset_load_state();
        return 0;
    case 0x801c6d90U:
        load_white_clut();
        return 0;
    case 0x801c6e0cU:
        set_up_labels();
        return 0;
    case 0x801c6e68U:
        read_sheet_entries();
        return 0;
    case 0x801c6f70U:
        set_up_frame_primitives();
        return 0;
    case 0x801c72bcU:
        load_menu_data_set(arg(0));
        return 0;
    case 0x801c7b0cU:
        set_up_screen();
        return 0;
    case 0x801c7bf4U:
        menu_frame();
        return 0;
    case 0x801c7d78U:
        decode_input();
        return 0;
    case 0x801c7f34U:
        split_play_time(arg(0));
        return 0;
    case 0x801c80b8U:
        split_decimal_digits(arg(0));
        return 0;
    case 0x801c8164U:
        set_gradient_quad(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801c81e0U:
        start_slide(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        return 0;
    case 0x801c8324U:
        step_slide(arg(0));
        return 0;
    case 0x801c851cU:
        set_screen_quad_vectors(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801c8574U:
        play_menu_sound(arg(0));
        return 0;
    case 0x801c865cU:
        return party_slot_bit(arg(0), arg(1));
    case 0x801c8694U:
        check_loaded_disc(arg(0));
        return 0;
    case 0x801c87c4U:
        undeliver_card_events();
        return 0;
    case 0x801c881cU:
        return wait_card_event();
    case 0x801c891cU:
        return card_status(arg(0));
    case 0x801c8960U:
        close_card_events();
        return 0;
    case 0x801c8a10U:
        return poll_card_port(arg(0));
    case 0x801c8becU:
        poll_cards();
        return 0;
    case 0x801c8d78U:
        return list_card_directory(arg(0));
    case 0x801c8ee8U:
        list_unscanned_ports();
        return 0;
    case 0x801c9038U:
        return read_file_head(arg(0), arg(1));
    case 0x801c90b0U:
        load_file_head(arg(0), arg(1));
        return 0;
    case 0x801c9270U:
        mark_game_files(arg(0));
        return 0;
    case 0x801c93a8U:
        return refresh_cards();
    case 0x801c9bccU:
        return cursor_slot_suits(arg(0));
    case 0x801c9d34U:
        return find_suitable_slot(arg(0));
    case 0x801c9ef4U:
        card_cursor_down(arg(0), arg(1));
        return 0;
    case 0x801ca750U:
        return card_cursor_input(arg(0));
    case 0x801ca8c0U:
        build_save_title(arg(0));
        return 0;
    case 0x801caa38U:
        return confirm_choice(arg(0));
    case 0x801cacf8U:
        return ask_confirmation(arg(0), arg(1), arg(2));
    case 0x801cadb0U:
        reset_card_cursor();
        return 0;
    case 0x801cae08U:
        card_access_indicator(arg(0));
        return 0;
    case 0x801cb0a8U:
        close_access_indicator(arg(0));
        return 0;
    case 0x801cb184U:
        decode_game_names();
        return 0;
    case 0x801cb28cU:
        apply_loaded_payload(arg(0));
        return 0;
    case 0x801cb304U:
        return load_game();
    case 0x801cb8acU:
        return ask_format_card(arg(0));
    case 0x801cb9e8U:
        return choose_save_digit(arg(0), arg(1));
    case 0x801cba4cU:
        build_save_payload(arg(0), arg(1), arg(2));
        return 0;
    case 0x801cbd90U:
        return save_game(arg(0));
    case 0x801cbdbcU:
        return save_game_body(arg(0), arg(1), arg(2), arg(3));
    case 0x801cd710U:
        return run_file_command(arg(0));
    case 0x801cd81cU:
        status_panel_layout_sprites(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        return 0;
    case 0x801cdb1cU:
        status_panel_byte62_digits(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801cdc6cU:
        status_panel_values(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        return 0;
    case 0x801ce024U:
        status_panel_values_tail(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801ce0ccU:
        build_status_panel(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        return 0;
    case 0x801ce198U:
        project_quads(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801ce2b4U:
        draw_packets(arg(0), arg(1), arg(2));
        return 0;
    case 0x801ce338U:
        draw_header();
        return 0;
    case 0x801ce3c8U:
        draw_cursors();
        return 0;
    case 0x801ce464U:
        draw_lists_340_344();
        return 0;
    case 0x801ce540U:
        draw_party_windows();
        return 0;
    case 0x801ce660U:
        draw_status_quads();
        return 0;
    case 0x801ce860U:
        draw_rows_35c();
        return 0;
    case 0x801ceb5cU:
        draw_row_block();
        return 0;
    case 0x801cebb4U:
        draw_windows_360();
        return 0;
    case 0x801cec40U:
        draw_screen_image();
        return 0;
    case 0x801cf308U:
        draw_lists_354();
        return 0;
    case 0x801cf37cU:
        draw_slot_cursors();
        return 0;
    case 0x801cf5e4U:
        draw_slot_icons(arg(0), arg(1), arg(2));
        return 0;
    case 0x801cf8d8U:
        draw_card_headers();
        return 0;
    case 0x801cfb48U:
        draw_slot_lines();
        return 0;
    case 0x801cff64U:
        draw_scroll_strip();
        return 0;
    case 0x801d01d0U:
        draw_file_screen();
        return 0;
    case 0x801d02d8U:
        draw_slot_list();
        return 0;
    case 0x801d0954U:
        project_panel_piece(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d09f0U:
        draw_panel(arg(0), arg(1));
        return 0;
    case 0x801d0c78U:
        draw_panels();
        return 0;
    case 0x801d0d90U:
        draw_state_windows_4e0();
        return 0;
    case 0x801d0e20U:
        return delay_seven();
    case 0x801d0e38U:
        draw_state_windows_ae0();
        return 0;
    case 0x801d0ebcU:
        return delay_five();
    case 0x801d0ed4U:
        draw_state_windows_10e0();
        return 0;
    case 0x801d0f54U:
        draw_state_windows_14e0();
        return 0;
    case 0x801d0fd4U:
        draw_state_window_17e0();
        return 0;
    case 0x801d1030U:
        draw_windows_1de0();
        return 0;
    case 0x801d10dcU:
        draw_state_windows_18e0();
        return 0;
    case 0x801d1160U:
        draw_state_windows_1be0();
        return 0;
    case 0x801d11f0U:
        draw_state_windows();
        return 0;
    case 0x801d1258U:
        link_fade();
        return 0;
    case 0x801d12d4U:
        draw_list_window(arg(0), arg(1));
        return 0;
    case 0x801d13f8U:
        draw_list_windows();
        return 0;
    case 0x801d1464U:
        draw_window_43c();
        return 0;
    case 0x801d14b0U:
        draw_quads_440();
        return 0;
    case 0x801d14fcU:
        draw_rows_42c();
        return 0;
    case 0x801d1640U:
        draw_rows_430();
        return 0;
    case 0x801d17c4U:
        draw_rows_434();
        return 0;
    case 0x801d1914U:
        draw_rows_438();
        return 0;
    case 0x801d1aacU:
        draw_windows_444();
        return 0;
    case 0x801d1b20U:
        draw_field_menu_screen();
        return 0;
    case 0x801d1be8U:
        draw_title_file_screen();
        return 0;
    case 0x801d1ca0U:
        draw_screen();
        return 0;
    case 0x801d1d40U:
        update_view();
        return 0;
    case 0x801d1e80U:
        zoom_in();
        return 0;
    case 0x801d1eb0U:
        zoom_out();
        return 0;
    case 0x801d1ee0U:
        draw_highlight(arg(0), arg(1));
        return 0;
    case 0x801d22c4U:
        hide_cursors();
        return 0;
    case 0x801d22f4U:
        set_markers(arg(0));
        return 0;
    case 0x801d2484U:
        clear_markers();
        return 0;
    case 0x801d28a8U:
        open_amount_window();
        return 0;
    case 0x801d28fcU:
        open_play_time_window();
        return 0;
    case 0x801d2968U:
        draw_status_panel();
        return 0;
    case 0x801d29a8U:
        slide_party_panels(arg(0), arg(1));
        return 0;
    case 0x801d2d38U:
        open_field_menu();
        return 0;
    case 0x801d2f4cU:
        show_message(arg(0));
        return 0;
    case 0x801d32b4U:
        close_message();
        return 0;
    case 0x801d3344U:
        draw_screen_title(arg(0), arg(1), arg(2));
        return 0;
    case 0x801d3444U:
        release_screen_title();
        return 0;
    case 0x801d3488U:
        draw_party_window_sprites(arg(0), arg(1));
        return 0;
    case 0x801d3674U:
        release_help_block();
        return 0;
    case 0x801d36e0U:
        draw_portrait_panel(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d397cU:
        open_window(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5), arg(6), arg(7), arg(8));
        return 0;
    case 0x801d3b00U:
        grow_opening_panels();
        return 0;
    case 0x801d3c4cU:
        build_panel_title(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801d3db0U:
        build_panel_corners(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801d3ff8U:
        build_panel_top_edge(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d433cU:
        build_panel_bottom_edge(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801d4688U:
        build_panel_left_edge(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d49d0U:
        build_panel_right_edge(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801d4d1cU:
        build_panel(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5), arg(6), arg(7));
        return 0;
    case 0x801d4ea0U:
        release_text_blocks(arg(0));
        return 0;
    case 0x801d4f2cU:
        draw_panel_portrait(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d50ecU:
        draw_panel_labels(arg(0), arg(1), arg(2));
        return 0;
    case 0x801d51ecU:
        draw_panel_numbers_4c_4e(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d53d0U:
        draw_panel_numbers_50_52(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d55b4U:
        draw_panel_numbers_44_48(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d5794U:
        draw_panel_numbers_62_63(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d5a50U:
        draw_party_panel(arg(0), arg(1));
        return 0;
    case 0x801d5ba4U:
        draw_amount(arg(0), arg(1));
        return 0;
    case 0x801d5cf8U:
        draw_play_time(arg(0), arg(1));
        return 0;
    case 0x801d5ed4U:
        draw_detail_portrait(arg(0), arg(1));
        return 0;
    case 0x801d680cU:
        draw_detail_numbers(arg(0), arg(1));
        return 0;
    case 0x801d7f50U:
        draw_stat_names(arg(0), arg(1), arg(2));
        return 0;
    case 0x801d827cU:
        make_stat_bar(arg(0), arg(1));
        return 0;
    case 0x801d83acU:
        tint_sprite_parts(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d84b4U:
        measure_stat_bar(arg(0), arg(1), arg(2));
        return 0;
    case 0x801d85dcU:
        return largest_stat(arg(0), arg(1), arg(2));
    case 0x801d8644U:
        draw_stat_bars(arg(0), arg(1), arg(2), arg(3), arg(4));
        return 0;
    case 0x801d8de4U:
        draw_stat_panel(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d8ea4U:
        draw_part_panel(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801d9808U:
        return sound_mode_screen();
    case 0x801d9b08U:
        restart_card_access();
        return 0;
    case 0x801d9c84U:
        return enter_card_mode();
    case 0x801d9e3cU:
        leave_card_mode();
        return 0;
    case 0x801d9f34U:
        draw_file_labels();
        return 0;
    case 0x801d9f98U:
        return file_screen(arg(0), arg(1));
    case 0x801da4a8U:
        open_item_screen();
        return 0;
    case 0x801da518U:
        close_equipment_shared();
        return 0;
    case 0x801da5bcU:
        build_item_list(arg(0));
        return 0;
    case 0x801da9a8U:
        show_item_description(arg(0), arg(1));
        return 0;
    case 0x801db02cU:
        open_cursor_sprite(arg(0));
        return 0;
    case 0x801db0a8U:
        draw_cursor_sprite(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801db340U:
        release_cursor_sprite(arg(0));
        return 0;
    case 0x801db39cU:
        show_item_screen_texts(arg(0));
        return 0;
    case 0x801db5e4U:
        build_target_panels(arg(0));
        return 0;
    case 0x801db920U:
        return use_item_on_targets(arg(0), arg(1));
    case 0x801dbdb4U:
        measure_item_list(arg(0), arg(1));
        return 0;
    case 0x801dbe54U:
        return item_screen();
    case 0x801de2c8U:
        open_equipment_list(arg(0));
        return 0;
    case 0x801de474U:
        draw_equipment_frames(arg(0), arg(1));
        return 0;
    case 0x801de5ccU:
        return build_candidate_list(arg(0), arg(1), arg(2), arg(3), arg(4));
    case 0x801df0d4U:
        return commit_equipment(arg(0), arg(1), arg(2), arg(3));
    case 0x801df5d0U:
        keep_equipped_parts(arg(0), arg(1));
        return 0;
    case 0x801dfb68U:
        preview_candidate(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
        return 0;
    case 0x801dff5cU:
        draw_part_description(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5), arg(6));
        return 0;
    case 0x801e05d0U:
        equipment_screen_loop(arg(0), arg(1), arg(2));
        return 0;
    case 0x801e0f78U:
        return run_equipment_screen(arg(0), arg(1));
    case 0x801e3088U:
        leave_menu_screen(arg(0));
        return 0;
    case 0x801e31c0U:
        return apply_consumable(arg(0), arg(1), arg(2));
    case 0x801e36d4U:
        rebuild_equipment_bonuses(arg(0), arg(1));
        return 0;
    case 0x801e3a80U:
        update_equipment_stats(arg(0), arg(1));
        return 0;
    case 0x801e41c0U:
        recompute_gear_engine(arg(0), arg(1));
        return 0;
    case 0x801e4258U:
        recompute_gear_model(arg(0), arg(1));
        return 0;
    case 0x801e42acU:
        recompute_gear_frame(arg(0), arg(1));
        return 0;
    case 0x801e433cU:
        recompute_gear_parts(arg(0), arg(1));
        return 0;
    case 0x801e4928U:
        return gear_part_level(arg(0));
    case 0x801e4a28U:
        store_payload_game_data(arg(0));
        return 0;
    case 0x801e4d10U:
        restore_payload_game_data(arg(0), arg(1));
        return 0;
    case 0x801e53ccU:
        prepare_window_packets(arg(0));
        return 0;
    case 0x801e56e8U:
        build_file_slot_marker(arg(0));
        return 0;
    case 0x801e5924U:
        build_file_slot_outline(arg(0));
        return 0;
    case 0x801e5accU:
        build_file_slots();
        return 0;
    case 0x801e5b3cU:
        release_file_blocks();
        return 0;
    case 0x801e5b88U:
        build_file_glyphs();
        return 0;
    case 0x801e5e4cU:
        build_file_banner();
        return 0;
    case 0x801e61b0U:
        draw_details_labels();
        return 0;
    case 0x801e6450U:
        build_file_select_panels();
        return 0;
    case 0x801e649cU:
        release_details_block();
        return 0;
    case 0x801e64e0U:
        clear_file_title();
        return 0;
    case 0x801e6544U:
        narrow_glyph_rows(arg(0));
        return 0;
    case 0x801e65e4U:
        return kanji_glyph_address(arg(0));
    case 0x801e6668U:
        draw_file_title(arg(0));
        return 0;
    case 0x801e68acU:
        draw_details_time(arg(0));
        return 0;
    case 0x801e6ae8U:
        draw_details_portrait(arg(0), arg(1));
        return 0;
    case 0x801e6b70U:
        draw_details_level(arg(0), arg(1));
        return 0;
    case 0x801e6cfcU:
        draw_details_hp(arg(0), arg(1));
        return 0;
    case 0x801e6f5cU:
        draw_details_values(arg(0), arg(1));
        return 0;
    case 0x801e71b4U:
        draw_details_name(arg(0), arg(1), arg(2));
        return 0;
    case 0x801e733cU:
        build_title_strip();
        return 0;
    case 0x801e76ecU:
        draw_file_details(arg(0));
        return 0;
    case 0x801e781cU:
        show_file_details(arg(0), arg(1));
        return 0;
    case 0x801e78c8U:
        load_file_icon(arg(0));
        return 0;
    case 0x801e7c50U:
        set_label_packets(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801e7e68U:
        layout_label_pairs(arg(0), arg(1), arg(2), arg(3));
        return 0;
    case 0x801e8018U:
        layout_labels_row4(arg(0), arg(1), arg(2));
        return 0;
    case 0x801e8044U:
        clear_bytes(arg(0), arg(1));
        return 0;
    case 0x801e8070U:
        place_label(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5), arg(6), arg(7));
        return 0;
    case 0x801e8474U:
        reveal_sprite_columns(arg(0), arg(1));
        return 0;
    case 0x801e86c8U:
        reveal_row_list(arg(0));
        return 0;
    case 0x801e8978U:
        build_sprite_columns(arg(0), arg(1), arg(2));
        return 0;
    case 0x801e8b4cU:
        build_row_list(arg(0));
        return 0;
    case 0x801e8da8U:
        load_character_name(arg(0), arg(1));
        return 0;
    case 0x801e8eacU:
        shade_quad(arg(0), arg(1));
        return 0;
    case 0x801e8f60U:
        shade_window_quads(arg(0), arg(1));
        return 0;
    case 0x801e91c4U:
        set_quad_translucent(arg(0));
        return 0;
    case 0x801e920cU:
        set_quad_rect(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5), arg(6));
        return 0;
    case 0x801e927cU:
        init_text_quad(arg(0));
        return 0;
    default:
        missing("overlay_call", address, "symbol:menu-entry",
                "The overlay function at this entry is not reconstructed");
    }
}

} // namespace xem::reconstruction::menu

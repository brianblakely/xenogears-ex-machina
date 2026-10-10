/* Resident symbols the world map source addresses from their base, where the
 * generated undefined-symbol list only names the accessed fields. */
game_data = 0x8006D634; /* game data */
/* Members of the game data (resident/gamedata.h GameData) by names of the
 * world map's own. */
game_data_parked_vehicle0_heading = game_data + 0x1826; /* 8006EE5A: worldmap.unk5A */
game_data_parked_vehicle1_heading = game_data + 0x1828; /* 8006EE5C: worldmap.unk5C */
game_data_parked_vehicle2_heading = game_data + 0x182A; /* 8006EE5E: worldmap.unk5E */
game_data_flying_vehicle_heading = game_data + 0x1832; /* 8006EE66: worldmap.vehicle_heading */
game_data_worldmap_return_flags = game_data + 0x1834; /* 8006EE68: worldmap.flags */
game_data_vehicle_spots_by_follower = game_data + 0x1956; /* 8006EF8A: vars[0x13] */
game_data_vehicle_spots = game_data + 0x195A; /* 8006EF8E: vars[0x15] */
game_data_worldmap_flag_word = game_data + 0x2320; /* 8006F954: entry[2] */

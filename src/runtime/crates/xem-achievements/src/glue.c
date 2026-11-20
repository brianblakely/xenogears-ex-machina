/* What the Rust side takes from rcheevos through C (src/ffi.rs): the
 * layouts of the structures it mirrors, as this target's C compiler lays
 * them out (compared in `ffi::tests`), and a disc's hash, whose iterator's
 * layout depends on the hashing options build.rs compiles with. */
#include <stdio.h>
#include <string.h>

#include "rc_client.h"
#include "rc_consoles.h"
#include "rc_hash.h"

size_t xem_rc_layout(size_t* out, size_t capacity)
{
  const size_t layout[] = {
    sizeof(rc_api_request_t), offsetof(rc_api_request_t, url), offsetof(rc_api_request_t, post_data),
    offsetof(rc_api_request_t, content_type),
    sizeof(rc_api_server_response_t), offsetof(rc_api_server_response_t, body),
    offsetof(rc_api_server_response_t, body_length), offsetof(rc_api_server_response_t, http_status_code),
    sizeof(rc_client_user_t), offsetof(rc_client_user_t, display_name), offsetof(rc_client_user_t, username),
    offsetof(rc_client_user_t, token), offsetof(rc_client_user_t, score), offsetof(rc_client_user_t, score_softcore),
    sizeof(rc_client_user_game_summary_t), offsetof(rc_client_user_game_summary_t, num_core_achievements),
    offsetof(rc_client_user_game_summary_t, num_unlocked_achievements), offsetof(rc_client_user_game_summary_t, points_core),
    offsetof(rc_client_user_game_summary_t, points_unlocked),
    sizeof(rc_client_game_t), offsetof(rc_client_game_t, id), offsetof(rc_client_game_t, title),
    offsetof(rc_client_game_t, hash),
    sizeof(rc_client_achievement_t), offsetof(rc_client_achievement_t, title),
    offsetof(rc_client_achievement_t, description), offsetof(rc_client_achievement_t, measured_progress),
    offsetof(rc_client_achievement_t, id), offsetof(rc_client_achievement_t, points),
    offsetof(rc_client_achievement_t, unlocked), offsetof(rc_client_achievement_t, badge_url),
    sizeof(rc_client_leaderboard_t), offsetof(rc_client_leaderboard_t, title),
    offsetof(rc_client_leaderboard_t, tracker_value),
    sizeof(rc_client_server_error_t), offsetof(rc_client_server_error_t, error_message),
    offsetof(rc_client_server_error_t, api), offsetof(rc_client_server_error_t, result),
    sizeof(rc_client_subset_t), offsetof(rc_client_subset_t, title),
    sizeof(rc_client_event_t), offsetof(rc_client_event_t, achievement), offsetof(rc_client_event_t, leaderboard),
    offsetof(rc_client_event_t, server_error), offsetof(rc_client_event_t, subset),
  };
  const size_t count = sizeof(layout) / sizeof(layout[0]);
  size_t i;
  for (i = 0; i < count && i < capacity; ++i)
    out[i] = layout[i];
  return count;
}

/* A disc as the Rust side reads it: `read` gives the user data of the
 * data track's sector `sector` (its Form 1 window), `bytes` at most, and
 * returns how many it gave. */
typedef size_t (*xem_read_sector_t)(void* source, uint32_t sector, void* buffer, size_t bytes);

typedef struct xem_disc_t {
  void* source;
  xem_read_sector_t read;
  char error[160];
} xem_disc_t;

/* The disc's one data track, which starts at its first sector. */
static void* xem_open_track(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
  (void)path;
  if (track != 1 && track != RC_HASH_CDTRACK_FIRST_DATA)
    return NULL;
  return iterator->userdata;
}

static size_t xem_read_track_sector(void* track, uint32_t sector, void* buffer, size_t bytes)
{
  xem_disc_t* disc = (xem_disc_t*)track;
  return disc->read(disc->source, sector, buffer, bytes);
}

static void xem_close_track(void* track)
{
  (void)track;
}

static uint32_t xem_first_track_sector(void* track)
{
  (void)track;
  return 0;
}

static void xem_hash_error(const char* message, const rc_hash_iterator_t* iterator)
{
  xem_disc_t* disc = (xem_disc_t*)iterator->userdata;
  snprintf(disc->error, sizeof(disc->error), "%s", message);
}

/* RetroAchievements' hash of a PlayStation disc (rcheevos' own, rc_hash_psx:
 * the boot program's name and contents), read through `read`. Returns
 * nonzero with the hash, else zero with why in `error`. */
int xem_rc_disc_hash(char hash[33], void* source, xem_read_sector_t read, char error[160])
{
  xem_disc_t disc;
  rc_hash_iterator_t iterator;
  int result;

  disc.source = source;
  disc.read = read;
  disc.error[0] = '\0';

  /* A path without an extension: rcheevos takes it as a disc image. */
  rc_hash_initialize_iterator(&iterator, "disc", NULL, 0);
  iterator.userdata = &disc;
  iterator.callbacks.verbose_message = NULL;
  iterator.callbacks.error_message = xem_hash_error;
  iterator.callbacks.cdreader.open_track = NULL;
  iterator.callbacks.cdreader.open_track_iterator = xem_open_track;
  iterator.callbacks.cdreader.read_sector = xem_read_track_sector;
  iterator.callbacks.cdreader.close_track = xem_close_track;
  iterator.callbacks.cdreader.first_track_sector = xem_first_track_sector;

  result = rc_hash_generate(hash, RC_CONSOLE_PLAYSTATION, &iterator);
  rc_hash_destroy_iterator(&iterator);

  memcpy(error, disc.error, sizeof(disc.error));
  return result;
}

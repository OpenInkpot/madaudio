#include <libintl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <Evas.h>
#include <Edje.h>
#include <Ecore_File.h>
#include <mpd/tag.h>
#include <mpd/song.h>
#include <mpd/status.h>
#include "madaudio.h"


static void
blank_gui(Evas_Object* gui)
{
    edje_object_part_text_set(gui, "title", "");
    edje_object_part_text_set(gui, "composer", "");
    edje_object_part_text_set(gui, "album", "");
    edje_object_part_text_set(gui, "artist", "");
    edje_object_part_text_set(gui, "genre", "");
    edje_object_part_text_set(gui, "year", "");
    edje_object_part_text_set(gui, "prev-song", "");
    edje_object_part_text_set(gui, "next-song", "");
    edje_object_part_text_set(gui, "this-song", "");
}

static void
fill_recorder_gui(Evas_Object *gui)
{
    edje_object_part_text_set(gui, "recorder-title", gettext("Recorder"));
    edje_object_part_text_set(gui, "recorder-help",
        gettext("Press OK to start recording"));
}

static void
draw_song_tag(Evas_Object* gui, const char *field, const struct mpd_song* song,
                enum mpd_tag_type type)
{
    const char* value = mpd_song_get_tag(song, type, 0);
    if(value)
    {
        edje_object_part_text_set(gui, field, value);
    }
}

static const char *
get_song_title(const struct mpd_song* song)
{
    const char* title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
    if(!title || !strlen(title))
    {
        title = mpd_song_get_uri(song);
        if(title)
            return ecore_file_file_get(title);
    }
    return title;
}

static void
draw_song_title(Evas_Object* gui, const struct mpd_song* song)
{
    const char* title = get_song_title(song);
    if(title)
        edje_object_part_text_set(gui, "title", title);
}

static const char*
get_title_by_num(madaudio_player_t* player, int offset)
{
    int pos = mpd_status_get_song_pos(player->status);
    pos += offset;
    if(pos < 0 || pos > mpd_status_get_queue_length(player->status))
        return "";
    struct mpd_song* song = eina_list_nth(player->playlist, pos);
    if(song)
        return get_song_title(song);
    return "";
}

static void
draw_song(Evas_Object* gui, const struct mpd_song* song)
{
    draw_song_title(gui, song);
    draw_song_tag(gui, "composer", song, MPD_TAG_COMPOSER);
    draw_song_tag(gui, "artist", song, MPD_TAG_ARTIST);
    draw_song_tag(gui, "album", song, MPD_TAG_ALBUM);
    draw_song_tag(gui, "genre", song, MPD_TAG_GENRE);
    draw_song_tag(gui, "year", song, MPD_TAG_DATE);
}

static const char *
format_time(int inttime)
{
    if(inttime < 0)
        return "unknown";
    static char buf[64];
    time_t time = inttime;
    const struct tm *tm = gmtime(&time);
    int min = tm->tm_hour * 60 + tm->tm_min;
    snprintf(buf, 64, "%d:%02d", min, tm->tm_sec);
    return buf;
}

static void
draw_prev_next(madaudio_player_t* player)
{

    Evas_Object* gui = player->gui;
    edje_object_part_text_set(gui, "prev-song", get_title_by_num(player, -1));
    edje_object_part_text_set(gui, "this-song", get_title_by_num(player, 0));
    edje_object_part_text_set(gui, "next-song", get_title_by_num(player, 1));
}

static void
draw_volume(Evas_Object* gui, const struct mpd_status* status)
{
    char buf[100];
    int volume = mpd_status_get_volume(status);
    if(volume == -1)
        volume = 0;
    snprintf(buf, 100, "volume-level,%d", volume );
    edje_object_signal_emit(gui, buf , "");
}

static void
draw_button(Evas_Object* gui, const char* button, bool state)
{
   char buf[100];
   snprintf(buf, 100, "%s,%s", button, state ? "pressed" : "default");
   edje_object_signal_emit(gui, buf , "");
}

static void
draw_status(Evas_Object* gui, const struct mpd_status* status)
{
    int time = mpd_status_get_total_time(status);
    enum mpd_state state = mpd_status_get_state(status);
    char* total = strdup(format_time(time));
    printf("time in status: %d ==\"%s\"\n", time, total);
    if(state == MPD_STATE_PLAY || state == MPD_STATE_PAUSE) {
        int elapsed_time = mpd_status_get_elapsed_time(status);
        char* elapsed = strdup(format_time(elapsed_time));
        char timestr[1024];
        snprintf(timestr, 1024, "%s / %s", elapsed, total);
        edje_object_part_text_set(gui, "total_time", timestr);
        free(elapsed);
        if(elapsed_time)
            elapsed_time = trunc(100 * elapsed_time/ time);
        madaudio_update_meter(gui, "meter", "meter", elapsed_time);
    }
    else
    {
        edje_object_part_text_set(gui, "total_time", format_time(time));
        madaudio_update_meter(gui, "meter", "meter", 0);
    }
    free(total);
    draw_button(gui, "playpause", (state == MPD_STATE_PLAY));
    draw_button(gui, "cycle", mpd_status_get_repeat(status));
    draw_button(gui, "full", mpd_status_get_single(status));
}

void
madaudio_draw_captions(madaudio_player_t* player)
{
    Evas_Object* gui = player->gui;
    edje_object_part_text_set(gui, "caption-composer", gettext("Composer"));
    edje_object_part_text_set(gui, "caption-artist", gettext("Artist"));
    edje_object_part_text_set(gui, "caption-album", gettext("Album"));
    edje_object_part_text_set(gui, "caption-genre", gettext("Genre"));
    edje_object_part_text_set(gui, "caption-year", gettext("Year"));
}

static void
madaudio_clear_captions(madaudio_player_t *player)
{
    Evas_Object* gui = player->gui;
    edje_object_part_text_set(gui, "caption-composer", "");
    edje_object_part_text_set(gui, "caption-artist", "");
    edje_object_part_text_set(gui, "caption-album", "");
    edje_object_part_text_set(gui, "caption-genre", "");
    edje_object_part_text_set(gui, "caption-year", "");
}

static void
madaudio_draw_statusbar(madaudio_player_t* player)
{
    if(!player->status)
    {
        printf("No status\n");
        return;
    }
    const char *error = mpd_status_get_error(player->status);
    if(error)
    {
        Evas* evas = evas_object_evas_get(player->gui);
        Evas_Object* edje = evas_object_name_find(evas, "main_edje");
        edje_object_part_text_set(edje, "footer", error);
        printf("mpd error: %s\n", error);
    }
}

void
madaudio_draw_song(madaudio_player_t* player)
{
    /* recorder has own "draw" */
    if(player->recorder)
        return;

    madaudio_draw_captions(player);
    blank_gui(player->gui);
    if(!player->conn || !player->status)
        return;
    draw_status(player->gui, player->status);
    draw_volume(player->gui, player->status);
    draw_prev_next(player);
    if(player->playlist)
    {
        int song_id = mpd_status_get_song_pos(player->status);
        struct mpd_song* song = eina_list_nth(player->playlist, song_id);
        if(song)
            draw_song(player->gui, song);
        else
            printf("No song\n");
    }
    fill_recorder_gui(player->gui);
    madaudio_draw_statusbar(player);
}

static Eina_Bool
record_callback(void *data)
{
    madaudio_player_t *player = data;
    time_t current_time = time(NULL);
    double diff = difftime(current_time, player->recorder_start_time);
    edje_object_part_text_set(player->gui, "recorder-timer",
        format_time((int) diff));
}

void
madaudio_draw_recorder_start(madaudio_player_t *player)
{
    madaudio_update_freespace(player);
    madaudio_polling_stop(player);
    blank_gui(player->gui);
    madaudio_clear_captions(player);
    player->recorder_timer = ecore_timer_loop_add(5.0, record_callback, player);
    player->recorder_start_time =  time(NULL);
    player->recorder_current_time = player->recorder_start_time;
    edje_object_part_text_set(player->gui, "recorder-title",
        gettext("Recording..."));
    edje_object_part_text_set(player->gui, "recorder-help",
        gettext("Press C or OK to stop"));
    madaudio_update_meter(player->gui, "meter", "meter", 0);
    madaudio_update_meter(player->gui, "recorder-meter", "recorder-meter", 0);
    edje_object_signal_emit(player->gui, "recording-button", "start");
    record_callback(player);
}

void
madaudio_draw_recorder_stop(madaudio_player_t *player)
{
    ecore_timer_del(player->recorder_timer);
    player->recorder_timer = NULL;
    edje_object_signal_emit(player->gui, "recording-button", "stop");
    madaudio_update_freespace(player);
    madaudio_draw_song(player);
}

void
madaudio_draw_recorder_window(madaudio_player_t *player)
{
    madaudio_update_freespace(player);
    edje_object_part_text_set(player->gui, "recorder-timer", "");
    edje_object_part_text_set(player->gui, "recorder-freespace",
        format_time(player->freespace));
}

#include <bus.h>
#include <stddef.h>
#include <interface.h>
#include <config.h>

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                    sd_bus_message *value, void *userdata, sd_bus_error *error);
static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error);
static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error);
static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static const char object_path[] = "/org/clight/clight";
static const char bus_interface[] = "org.clight.clight";

static const sd_bus_vtable clight_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "s", get_version, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "t", NULL, offsetof(struct state, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Sunset", "t", NULL, offsetof(struct state, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Time", "i", NULL, offsetof(struct state, time), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("InEvent", "b", NULL, offsetof(struct state, in_event), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("DisplayState", "i", NULL, offsetof(struct state, display_state), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("PmState", "i", NULL, offsetof(struct state, pm_inhibited), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentBlPct", "d", NULL, offsetof(struct state, current_bl_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentKbdPct", "d", NULL, offsetof(struct state, current_kbd_pct), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentAmbientBr", "d", NULL, offsetof(struct state, ambient_br), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("CurrentTemp", "i", NULL, offsetof(struct state, current_temp), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(struct state, current_loc), SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD("Calibrate", NULL, NULL, method_calibrate, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Inhibit", "b", NULL, method_inhibit, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Location", "(dd)", get_location, offsetof(struct config, loc), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunrise", "s", NULL, offsetof(struct config, events[SUNRISE]), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Sunset", "s", NULL, offsetof(struct config, events[SUNSET]), SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_WRITABLE_PROPERTY("NoAutoCalib", "b", NULL, set_auto_calib, offsetof(struct config, no_auto_calib), 0),
    SD_BUS_WRITABLE_PROPERTY("NoKbdCalib", "b", NULL, NULL, offsetof(struct config, no_keyboard_bl), 0),
    SD_BUS_WRITABLE_PROPERTY("AmbientGamma", "b", NULL, NULL, offsetof(struct config, ambient_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothBacklight", "b", NULL, NULL, offsetof(struct config, no_smooth_backlight), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerEnter", "b", NULL, NULL, offsetof(struct config, no_smooth_dimmer[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothDimmerExit", "b", NULL, NULL, offsetof(struct config, no_smooth_dimmer[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("NoSmoothGamma", "b", NULL, NULL, offsetof(struct config, no_smooth_gamma), 0),
    SD_BUS_WRITABLE_PROPERTY("NumCaptures", "i", NULL, NULL, offsetof(struct config, num_captures), 0),
    SD_BUS_WRITABLE_PROPERTY("SensorName", "s", NULL, NULL, offsetof(struct config, dev_name), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightSyspath", "s", NULL, NULL, offsetof(struct config, screen_path), 0),
    SD_BUS_WRITABLE_PROPERTY("EventDuration", "i", NULL, NULL, offsetof(struct config, event_duration), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerPct", "d", NULL, NULL, offsetof(struct config, dimmer_pct), 0),
    SD_BUS_WRITABLE_PROPERTY("Verbose", "b", NULL, NULL, offsetof(struct config, verbose), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransStep", "d", NULL, NULL, offsetof(struct config, backlight_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepEnter", "d", NULL, NULL, offsetof(struct config, dimmer_trans_step[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransStepExit", "d", NULL, NULL, offsetof(struct config, dimmer_trans_step[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransStep", "i", NULL, NULL, offsetof(struct config, gamma_trans_step), 0),
    SD_BUS_WRITABLE_PROPERTY("BacklightTransDuration", "i", NULL, NULL, offsetof(struct config, backlight_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaTransDuration", "i", NULL, NULL, offsetof(struct config, gamma_trans_timeout), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationEnter", "i", NULL, NULL, offsetof(struct config, dimmer_trans_timeout[ENTER]), 0),
    SD_BUS_WRITABLE_PROPERTY("DimmerTransDurationExit", "i", NULL, NULL, offsetof(struct config, dimmer_trans_timeout[EXIT]), 0),
    SD_BUS_WRITABLE_PROPERTY("DayTemp", "i", NULL, set_gamma, offsetof(struct config, temp[DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("NightTemp", "i", NULL, set_gamma, offsetof(struct config, temp[NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattCurvePoints", "ad", get_curve, set_curve, offsetof(struct config, regression_points[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("ShutterThreshold", "d", NULL, NULL, offsetof(struct config, shutter_threshold), 0),
    SD_BUS_WRITABLE_PROPERTY("GammaLongTransition", "b", NULL, NULL, offsetof(struct config, gamma_long_transition), 0),
    SD_BUS_METHOD("Store", NULL, NULL, method_store_conf, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable conf_to_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_WRITABLE_PROPERTY("AcDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_AC][SIZE_STATES]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDayCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][DAY]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattNightCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][NIGHT]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattEventCapture", "i", NULL, set_timeouts, offsetof(struct config, timeout[ON_BATTERY][SIZE_STATES]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDimmer", "i", NULL, set_timeouts, offsetof(struct config, dimmer_timeout[ON_BATTERY]), 0),
    SD_BUS_WRITABLE_PROPERTY("AcDpms", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeout[ON_AC]), 0),
    SD_BUS_WRITABLE_PROPERTY("BattDpms", "i", NULL, set_timeouts, offsetof(struct config, dpms_timeout[ON_BATTERY]), 0),
    SD_BUS_VTABLE_END
};

const char *interface_temp_topic = "InterfaceTemp";
const char *interface_dimmer_to_topic = "InterfaceDimmerTo";
const char *interface_dpms_to_topic = "InterfaceDPMSTo";

MODULE("INTERFACE");

static void init(void) {
    const char conf_path[] = "/org/clight/clight/Conf";
    const char conf_to_path[] = "/org/clight/clight/Conf/Timeouts";
    const char conf_interface[] = "org.clight.clight.Conf";

    sd_bus *userbus = get_user_bus();
    /* Main interface */
    int r = sd_bus_add_object_vtable(userbus,
                                NULL,
                                object_path,
                                bus_interface,
                                clight_vtable,
                                &state);

    /* Conf interface */
    r += sd_bus_add_object_vtable(userbus,
                                NULL,
                                conf_path,
                                conf_interface,
                                conf_vtable,
                                &conf);

    /* Conf/Timeouts interface */
    r += sd_bus_add_object_vtable(userbus,
                                  NULL,
                                  conf_to_path,
                                  conf_interface,
                                  conf_to_vtable,
                                  &conf);
}

static bool check(void) {
    return true;
}

static bool evaluate() {
    return true;
}

static void receive(const msg_t *const msg, const void* userdata) {
    
}

static void destroy(void) {
    sd_bus *userbus = get_user_bus();
    sd_bus_release_name(userbus, bus_interface);
}

static int get_version(sd_bus *b, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append(reply, "s", VERSION);
}

static int method_calibrate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -EINVAL;

    // FIXME
//     if (is_running(BACKLIGHT)) {
//         FILL_MATCH_NONE();
//         r = sd_bus_reply_method_return(m, NULL);
//     } else {
//         sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Backlight module is not running.");
//     }
    return r;
}

static int method_inhibit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int inhibited;
    int r = sd_bus_message_read(m, "b", &inhibited);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    int old_inhibited = state.pm_inhibited;
    if (inhibited) {
        state.pm_inhibited |= PM_FORCED_ON;
        INFO("PowerManagement inhibition enabled by bus API.\n");
    } else {
        state.pm_inhibited &= ~PM_FORCED_ON;
        INFO("PowerManagement inhibition disabled by bus API.\n");
    }
    if (old_inhibited != state.pm_inhibited) {
        FILL_MATCH_NONE();
        emit_prop("PmState");
    }
    return sd_bus_reply_method_return(m, NULL);
}

static int get_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    return sd_bus_message_append_array(reply, 'd', userdata, SIZE_POINTS * sizeof(double));
}

static int set_curve(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    const double *data = NULL;
    size_t length;
    int r = sd_bus_message_read_array(value, 'd', (const void**) &data, &length);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    if (length / sizeof(double) != SIZE_POINTS) {
        WARN("Wrong parameters.\n");
        sd_bus_error_set_const(error, SD_BUS_ERROR_FAILED, "Wrong parameters.");
        r = -EINVAL;
    } else {
        enum ac_states ac_state = ON_AC;

        if (userdata == conf.regression_points[ON_BATTERY]) {
            ac_state = ON_BATTERY;
        }
        memcpy(conf.regression_points[ac_state], data, length);
        FILL_MATCH_DATA(ac_state);
    }
    return r;
}

static int get_location(sd_bus *bus, const char *path, const char *interface, const char *property,
                        sd_bus_message *reply, void *userdata, sd_bus_error *error) {
    struct location *l = (struct location *)userdata;
    return sd_bus_message_append(reply, "(dd)", l->lat, l->lon);
}

static int set_timeouts(sd_bus *bus, const char *path, const char *interface, const char *property,
                            sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int *val = (int *)userdata;
    int old_val = *val;

    int r = sd_bus_message_read(value, "i", userdata);
    if (r < 0) {
        WARN("Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }

    /* Check if we modified currently used timeout! */
    if (val == &conf.timeout[state.ac_state][state.time]) {
        FILL_MATCH_DATA_NAME(old_val, "backlight_timeout");
    } else if (val == &conf.dimmer_timeout[state.ac_state]) {
        FILL_MATCH_DATA_NAME(old_val, "dimmer_timeout");
    } else if (val == &conf.dpms_timeout[state.ac_state]) {
        FILL_MATCH_DATA_NAME(old_val, "dpms_timeout");
    }
    return r;
}

static int set_gamma(sd_bus *bus, const char *path, const char *interface, const char *property,
                     sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int old_val = *(int *)userdata;
    int r = sd_bus_message_read(value, "i", userdata);
    if (!r && old_val != *(int *)userdata) {
        FILL_MATCH_NONE();
    }
    return r;
}

static int set_auto_calib(sd_bus *bus, const char *path, const char *interface, const char *property,
                          sd_bus_message *value, void *userdata, sd_bus_error *error) {
    int old_val = conf.no_auto_calib;
    int r = sd_bus_message_read(value, "b", userdata);
    if (!r && old_val != conf.no_auto_calib) {
        FILL_MATCH_NONE();
    }
    return r;
}

static int method_store_conf(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = -1;
    if (store_config(LOCAL) == 0) {
        r = sd_bus_reply_method_return(m, NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Failed to store conf.");
    }
    return r;
}

int emit_prop(const char *signal) {
    //     if (is_running((self.idx))) {
    //         sd_bus *userbus = get_user_bus();
    //         if (userbus) {
    //             sd_bus_emit_properties_changed(userbus, object_path, bus_interface, signal, NULL);
    //             return 0;
    //         }
    //     }
}

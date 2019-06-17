#include <bus.h>

static int upower_check(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static const char *topic = "UPower";

MODULE("UPOWER");

static void init(void) {
    upower_init();
    m_register_topic(topic);
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    /* Start as soon as upower becomes available */
    return upower_check() == 0;
}

static void receive(const msg_t *const msg, const void* userdata) {
    
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int upower_check(void) {
    /* check initial AC state */
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");
    int r = get_property(&args, "b", &state.ac_state);
    return -(r < 0);
}

static int upower_init(void) {
    SYSBUS_ARG(args, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_upower_change);
}

/*
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");

    /*
     * Store last ac_state in old struct to be matched against new one
     * as we cannot be sure that a OnBattery changed signal has been really sent:
     * our match will receive these signals:
     * .DaemonVersion                      property  s         "0.99.5"     emits-change
     * .LidIsClosed                        property  b         true         emits-change
     * .LidIsPresent                       property  b         true         emits-change
     * .OnBattery                          property  b         false        emits-change
     */
    int old_ac_state = state.ac_state;
    int r = get_property(&args, "b", &state.ac_state);
    if (!r && old_ac_state != state.ac_state) {
        INFO(state.ac_state ? "Ac cable disconnected. Powersaving mode enabled.\n" : "Ac cable connected. Powersaving mode disabled.\n");
        m_publish_str(topic, "Changed");
    }
    return 0;
}

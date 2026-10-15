#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include "../src/runtime_pad.c"
#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#endif

static int capture;
int gow3gpu_overlay_captures_input(void) { return capture; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

static void inject(const char *path, const char *tokens) {
    FILE *f=fopen(path,"w");
    assert(f);
    fputs(tokens,f);
    fclose(f);
    usleep(25000);
}
static PadData read_pad(void) {
    PadData d;
    assert(pad_read_state(1,&d)==0);
    return d;
}
/* One finger in the middle of the left (480) or right (1440) half of the touchpad. */
static int touch_at(const PadData *d, int x) {
    return d->touch_count==1 && d->touches[0].x==x && d->touches[0].y==471;
}
static void pump(void) { SDL_UpdateJoysticks(); SDL_UpdateGamepads(); }
static int near(float a, float b, float tolerance) { return fabsf(a-b)<=tolerance; }
/* issn: the light bar of the virtual pad, as the game set it through SDL. */
static uint8_t led[3]; static int led_calls;
static bool set_led(void *userdata, Uint8 r, Uint8 g, Uint8 b) {
    (void)userdata; led[0]=r; led[1]=g; led[2]=b; ++led_calls; return true;
}

int main(void) {
    /* issn: the orientation integrates the gyroscope: 90 degrees about y in 1 s. */
    {
        float q[4]={0,0,0,1};
        const float w[3]={0.0f,1.5707963f,0.0f};
        for (int i=0;i<100;++i) integrate_orientation(q,w,0.01f);
        assert(near(q[0],0.0f,1e-3f) && near(q[1],0.70711f,2e-3f) && near(q[2],0.0f,1e-3f) && near(q[3],0.70711f,2e-3f));
        integrate_orientation(q,w,5.0f); /* a pause: kept */
        assert(near(q[1],0.70711f,2e-3f));
        float a[3];
        float lowest=10.0f, highest=-10.0f;
        for (uint64_t t=0;t<200000;t+=5000) { motion_shake(a,t,1.0f/STANDARD_GRAVITY); if (a[1]<lowest) lowest=a[1]; if (a[1]>highest) highest=a[1]; }
        assert(near(highest,3.5f,0.05f) && near(lowest,-1.5f,0.05f) && a[0]==0.0f && a[2]==0.0f);
    }
    const char *tmp=getenv("TMPDIR") ? getenv("TMPDIR") : getenv("TEMP") ? getenv("TEMP") : "/tmp";
    char path[1024];
    snprintf(path,sizeof(path),"%s/gow3-pad-test-XXXXXX",tmp);
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    setenv("GOW3_PAD_FILE",path,1);
    setenv("SDL_VIDEODRIVER","dummy",1);
    /* Only the virtual test controller is a gamepad, whatever is plugged in. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1d50/0x6189");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    assert(pad_init()==0 && pad_open(1,0,0,NULL)==1);

    /* A click without a touch surface (Back, Tab, Backspace, the pad file) plays like a finger:
     * the touch comes first, the button CLICK_LEAD_US later, and the touch stays CLICK_TAIL_US
     * after the release. God of War III opens photo mode on the release of a left click with
     * the finger still down; a touch that appears and vanishes with the button is ignored. */
    inject(path,"cross l3 touchpad_left");
    PadData data=read_pad();
    assert((data.buttons & (BTN_CROSS|BTN_L3))==(BTN_CROSS|BTN_L3));
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    inject(path,"");
    data=read_pad();
    assert(data.buttons==0 && touch_at(&data,480)); /* released: the finger lingers */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* A tap shorter than the lead still clicks, for CLICK_HOLD_US. */
    inject(path,"touchpad_right");
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    inject(path,"");
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* L1 + the right click is a left click (photo mode on a pad without a touchpad). The side
     * is chosen when the click starts and kept until it ends. */
    inject(path,"l1 touchpad_right");
    data=read_pad();
    assert((data.buttons & BTN_L1) && touch_at(&data,480));
    inject(path,"touchpad_right");
    data=read_pad();
    assert(!(data.buttons & BTN_L1) && touch_at(&data,480));
    inject(path,"");
    data=read_pad(); /* the release is seen here: the tail starts */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* The settings menu takes the input and ends a click; the source starts a new one after. */
    inject(path,"touchpad_right");
    usleep(60000);
    data=read_pad();
    assert(touch_at(&data,1440));
    capture=1;
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);
    capture=0;
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    inject(path,"");
    data=read_pad(); /* the release is seen here: the tail starts */
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    SDL_VirtualJoystickTouchpadDesc touch={.nfingers=2};
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.name="gow3 test controller";
    desc.vendor_id=0x1d50;
    desc.product_id=0x6189;
    desc.ntouchpads=1;
    desc.touchpads=&touch;
    SDL_VirtualJoystickSensorDesc sensor_desc[2]={{SDL_SENSOR_ACCEL,250.0f},{SDL_SENSOR_GYRO,250.0f}};
    desc.nsensors=2;
    desc.sensors=sensor_desc;
    desc.SetLED=set_led;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    assert(id!=0);
    SDL_Joystick *joystick=SDL_OpenJoystick(id);
    assert(joystick);
    /* A real touchpad: the fingers and the click go to the game as they are. */
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,0.75f,0.5f,1.0f));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,0.25f,1.0f,1.0f));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,true));
    pump();
    data=read_pad();
    assert(gamepad && data.touch_count==2 && (data.buttons & BTN_TOUCHPAD));
    assert(data.touches[0].x==1439 && data.touches[0].y==471 && data.touches[0].id==0);
    assert(data.touches[1].x==480 && data.touches[1].y==942 && data.touches[1].id==1);
    capture=1;
    data=read_pad();
    assert(data.touch_count==0 && data.buttons==0);
    capture=0;
    /* The click without a finger is played as a left click. */
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,0,0,0));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,0,0,0));
    pump();
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,false));
    pump();
    data=read_pad();
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);
    /* Back (Share, View) is the right click: the pause menu. With L1 it is the left click. */
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,true));
    pump();
    data=read_pad();
    assert(!(data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & BTN_TOUCHPAD) && touch_at(&data,1440));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,false));
    pump();
    data=read_pad();
    usleep(220000);
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,true));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,true));
    pump();
    data=read_pad();
    assert((data.buttons & BTN_L1) && !(data.buttons & BTN_TOUCHPAD) && touch_at(&data,480));
    usleep(60000);
    data=read_pad();
    assert((data.buttons & (BTN_L1|BTN_TOUCHPAD))==(BTN_L1|BTN_TOUCHPAD) && touch_at(&data,480));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,false));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_BACK,false));
    pump();
    data=read_pad();
    usleep(220000);
    data=read_pad();
    assert(data.buttons==0 && data.touch_count==0);

    /* issn: the pad's motion sensors, in G and rad/s, and the orientation from the gyroscope. */
    assert(sensors==(SENSOR_ACCEL|SENSOR_GYRO));
    const float tilt[3]={0.0f,STANDARD_GRAVITY,0.0f}, spin[3]={0.0f,1.5707963f,0.0f}, still[3]={0,0,0};
    assert(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_ACCEL,SDL_GetTicksNS(),tilt,3));
    assert(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_GYRO,SDL_GetTicksNS(),spin,3));
    pump();
    assert(pad_reset_orientation(1)==0);
    data=read_pad();
    assert(near(data.acceleration[1],1.0f,1e-3f) && near(data.angular_velocity[1],1.5707963f,1e-4f));
    usleep(200000);
    data=read_pad(); /* 0.2 s at 90 degrees/s: 18 degrees about y */
    assert(data.orientation[1]>0.10f && data.orientation[1]<0.25f && data.orientation[3]>0.96f);
    assert(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_GYRO,SDL_GetTicksNS(),still,3));
    pump();
    assert(pad_reset_orientation(1)==0);
    data=read_pad();
    assert(data.orientation[3]==1.0f && data.angular_velocity[1]==0.0f);
    /* scePadSetMotionSensorState(false): a pad at rest. */
    assert(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_GYRO,SDL_GetTicksNS(),spin,3));
    pump();
    assert(pad_motion_sensor_state(1,0)==0);
    data=read_pad();
    assert(data.angular_velocity[1]==0.0f && near(data.acceleration[1],1.0f,1e-6f));
    assert(pad_motion_sensor_state(1,1)==0);
    assert(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_GYRO,SDL_GetTicksNS(),still,3));
    pump();

    /* The shake source (G, a mapped button, the pad file's "shake"): motion, never a button. */
    inject(path,"shake");
    float lowest=10.0f, highest=-10.0f;
    for (int i=0;i<12;++i) {
        data=read_pad();
        assert(!(data.buttons & SHAKE));
        if (data.acceleration[1]<lowest) lowest=data.acceleration[1];
        if (data.acceleration[1]>highest) highest=data.acceleration[1];
        usleep(15000);
    }
    assert(highest-lowest>2.0f);
    inject(path,"");
    data=read_pad();
    assert(near(data.acceleration[1],1.0f,1e-3f));

    /* scePadSetLightBar reaches the pad once per color; scePadResetLightBar is user 1's blue. */
    const uint8_t red[3]={255,16,0};
    assert(pad_light_bar(1,red)==0 && led[0]==255 && led[1]==16 && led[2]==0);
    const int calls=led_calls;
    assert(pad_light_bar(1,red)==0 && led_calls==calls);
    assert(pad_reset_light_bar(1)==0 && led[0]==0 && led[1]==0 && led[2]==255);
    assert(pad_light_bar(1,NULL)==ERR_INVALID_ARG && pad_light_bar(7,red)==ERR_INVALID_HANDLE);
    assert(pad_get_handle(1,0,0)==1 && pad_get_handle(2,0,0)==ERR_INVALID_ARG);

    SDL_CloseJoystick(joystick);
    if (gamepad) SDL_CloseGamepad(gamepad);
    gamepad=NULL;
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    unlink(path);
    puts("PASS: pad ABI, staged left/right clicks, L1 chord, SDL touch coordinates, overlay capture, "
         "motion sensors, orientation, shake, light bar");
}

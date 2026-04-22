from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


OUT_DIR = Path("images")
OUT_DIR.mkdir(parents=True, exist_ok=True)


SNIPPETS = {
    "appendix_code_1_main_loop.png": """/* 主循环与事件调度：BareMode + OSAL 事件驱动（节选）
 * - 关键点：各周期事件以毫秒节拍触发，保证核心告警任务可抢占执行
 * - 说明：代码已做缩进统一与运算符校正（如 !=）
 */
static void Bare_FullFeatureLoop(void)
{
    u32 now;
    u32 t_net;
    u32 t_sensor;
    u32 t_oled;
    u32 t_link;

    BareMode_CommonHomeDraw();
    BareMode_CommonStateInit();
    osal_init();
    s_evt_tid_core = osal_add_task(AppEvt_CoreHandler, 0u);
    s_evt_tid_net = osal_add_task(AppEvt_NetHandler, 1u);
    s_evt_tid_sensor = osal_add_task(AppEvt_SensorHandler, 2u);
    s_evt_tid_oled = osal_add_task(AppEvt_OledHandler, 3u);
    s_evt_tid_link = osal_add_task(AppEvt_LinkHandler, 4u);

    now = Bare_GetTickMs();
    t_net = now; t_sensor = now; t_oled = now; t_link = now;

    for (;;)
    {
        now = Bare_GetTickMs();
        BareMode_ScanAndDispatchKey();

        if(s_evt_tid_core >= 0) osal_set_event((u8)s_evt_tid_core, EVT_CORE_STEP);
        if((s_evt_tid_net >= 0) && ((u32)(now - t_net) >= 2000u))
        { t_net = now; osal_set_event((u8)s_evt_tid_net, EVT_NET_2S); }
        if((s_evt_tid_sensor >= 0) && ((u32)(now - t_sensor) >= 1000u))
        { t_sensor = now; osal_set_event((u8)s_evt_tid_sensor, EVT_SENSOR_1S); }
        if((s_evt_tid_oled >= 0) && ((u32)(now - t_oled) >= (u32)OLED_DASHBOARD_PERIOD_MS))
        { t_oled = now; osal_set_event((u8)s_evt_tid_oled, EVT_OLED_REFRESH); }
        if((s_evt_tid_link >= 0) && ((u32)(now - t_link) >= 200u))
        { t_link = now; osal_set_event((u8)s_evt_tid_link, EVT_LINK_200MS); }

        while(osal_dispatch_once()) {}
        delay_ms(20);
    }
}""",
    "appendix_code_2_security_alarm.png": """/* 入侵告警触发/清除：联动执行 + UI 提示 + 抓拍 + 事件记录（节选） */
void Security_Alarm_Trigger(void)
{
    if(security_mode != SECURITY_ARMED) return;
    security_alarm = 1;
    RELAY = 1;
    RELAY_TIME = 0;
    OLED_BatchBegin();
    Security_Line0_Invalidate();
    Security_Mode_Show();
    OLED_ShowString(0, 16, OLED_S16_INTRU, 16);
    OLED_ShowString(0, 32, OLED_S16_PIR, 16);
    OLED_ShowString(0, 48, OLED_S16_UNLOCK, 16);
    OLED_BatchEnd();
    Security_Camera_Capture_Trigger();
    Linkage_OnIntrusion();
    SysLog_Add(LOG_EVT_ALARM, "INTRUSION");
}

void Security_Alarm_Clear(void)
{
    security_alarm = 0;
    security_alarm_reported = 0;
    BEEP_SoundOff();
    RELAY = 1;
    RELAY_TIME = 0;
    Security_Set_Mode(SECURITY_DISARMED);
    pass = 0;
    AuthLevel = 0;
    RegFingerprint = 0;
    ReInputEn = 0;
    ReInputReady = 0;
    PressNum = 0;
    InitDisplay = 1;
}""",
    "appendix_code_3_mq2_emergency.png": """/* MQ-2 燃气应急：进入应急状态并执行切断/告警/上报（节选） */
void MQ2_Emergency_Trigger(void)
{
    mq2_emergency_alarm = 1;
    OLED_BatchBegin();
    OLED_ShowString(0, 0, "GAS/SMOKE ALARM ", 16);
    OLED_ShowString(0, 16, "ENV OVERLIMIT   ", 16);
    OLED_ShowString(0, 32, "VALVE CLOSED    ", 16);
    OLED_ShowString(0, 48, OLED_S16_UNLOCK, 16);
    OLED_BatchEnd();
    Linkage_OnMQ2_Alarm();
    SysLog_Add(LOG_EVT_ALARM, "GAS_SMOKE");
}

void MQ2_Emergency_Run(void)
{
    if(mq2_emergency_alarm == 0) return;
    BEEP_SoundOn();
    Gas_Valve_Close();
    if(mq2_alarm_reported == 0)
    {
        mq2_alarm_reported = 1;
        Debug_ReportAlarm("GAS_SMOKE_OVERLIMIT");
    }
}""",
    "appendix_code_4_cloud_cmd.png": """/* 云端命令解析与执行：OneNET MQTT 控制量落地（节选） */
static void Bare_Period_WiFi(u32 now)
{
    (void)now;
#if EN_ESP8266_ONENET
    if(tls_inited == 1)
    {
        OneNET_Parse_Cmd();
        if(Ctrl_Door == 1)
        {
            Door_Open();
            Ctrl_Door = 0;
        }
        if(Ctrl_Arm == 1) Arm_System_Enable();
        else Arm_System_Disable();
        Led_Ctrl(Ctrl_Led);
    }
#endif
    UART_Remote_Ctrl();
    Security_Check_Serial_Command();
}""",
}


def get_font(size: int) -> ImageFont.FreeTypeFont:
    candidates = [
        # 优先使用 Courier New（论文要求等宽字体）
        "C:/Windows/Fonts/cour.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
    ]
    for c in candidates:
        p = Path(c)
        if p.exists():
            return ImageFont.truetype(str(p), size=size)
    return ImageFont.load_default()


def render_code_image(code: str, output: Path) -> None:
    # 生成图片会在论文中按高度缩放；使用较高像素字号以保证印刷清晰度
    font = get_font(30)
    lines = code.splitlines()
    padding_x = 48
    padding_y = 42
    line_gap = 14

    probe = Image.new("RGB", (100, 100), "white")
    draw = ImageDraw.Draw(probe)
    line_heights = []
    max_width = 0
    for line in lines:
        bbox = draw.textbbox((0, 0), line if line else " ", font=font)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        line_heights.append(h)
        max_width = max(max_width, w)

    content_height = sum(line_heights) + max(0, len(lines) - 1) * line_gap
    img_w = max_width + padding_x * 2
    img_h = content_height + padding_y * 2

    img = Image.new("RGB", (img_w, img_h), "white")
    draw = ImageDraw.Draw(img)
    y = padding_y
    for i, line in enumerate(lines):
        draw.text((padding_x, y), line, font=font, fill="black")
        y += line_heights[i] + line_gap

    img.save(output, format="PNG", optimize=True)


if __name__ == "__main__":
    for name, code in SNIPPETS.items():
        render_code_image(code, OUT_DIR / name)
    print(f"Generated {len(SNIPPETS)} images in {OUT_DIR}")

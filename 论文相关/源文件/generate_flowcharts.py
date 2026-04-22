import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Polygon
import matplotlib.patheffects as pe

plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "SimSun"]
plt.rcParams["axes.unicode_minus"] = False

THEME = {
    "edge": "#2D5B9E",
    "edge_soft": "#5E87C2",
    "fill_main": "#EEF4FF",
    "fill_sub": "#F7FAFF",
    "fill_warn": "#FFF3E8",
    "text": "#1F2D3D",
    "arrow": "#334E68",
}


def setup_canvas(ax, title):
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    ax.set_facecolor("#FFFFFF")
    ax.set_title(title, fontsize=17, fontweight="bold", color=THEME["text"], pad=14)


def add_box(ax, x, y, w, h, text, fc="#EAF2FF", ec="#2F5DAA", fs=10):
    box = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.02,rounding_size=0.025",
        linewidth=1.6,
        edgecolor=ec,
        facecolor=fc,
    )
    box.set_path_effects([
        pe.SimplePatchShadow(offset=(1.2, -1.2), shadow_rgbFace=(0, 0, 0), alpha=0.10),
        pe.Normal(),
    ])
    ax.add_patch(box)
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, color=THEME["text"])
    return box


def add_diamond(ax, cx, cy, w, h, text, fc="#FFF3E6", ec="#B86B00", fs=10):
    pts = [(cx, cy + h / 2), (cx + w / 2, cy), (cx, cy - h / 2), (cx - w / 2, cy)]
    d = Polygon(pts, closed=True, facecolor=fc, edgecolor=ec, linewidth=1.5)
    d.set_path_effects([
        pe.SimplePatchShadow(offset=(1.2, -1.2), shadow_rgbFace=(0, 0, 0), alpha=0.10),
        pe.Normal(),
    ])
    ax.add_patch(d)
    ax.text(cx, cy, text, ha="center", va="center", fontsize=fs, color=THEME["text"])
    return d


def arrow(ax, x1, y1, x2, y2, txt=None, fs=9):
    ax.annotate(
        "",
        xy=(x2, y2),
        xytext=(x1, y1),
        arrowprops=dict(
            arrowstyle="-|>",
            lw=1.5,
            color=THEME["arrow"],
            mutation_scale=12,
            shrinkA=2,
            shrinkB=2,
        ),
    )
    if txt:
        ax.text(
            (x1 + x2) / 2,
            (y1 + y2) / 2 + 0.02,
            txt,
            ha="center",
            va="bottom",
            fontsize=fs,
            color="#3A4A5A",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.9),
        )


def selection_matrix(path):
    # 关键技术选型矩阵：加大画布高度与字号，保证打印后文字可读（≥六号）
    fig, ax = plt.subplots(figsize=(10.5, 4.6))
    ax.axis("off")
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.set_facecolor("#FFFFFF")
    ax.text(0.5, 0.94, "关键技术选型矩阵（控制器 / 调度 / 协议 / 安全）",
            ha="center", va="center", fontsize=16, fontweight="bold", color=THEME["text"])

    def box(ax_, x, y, w, h, text, fc, ec, fs=13, fw="normal"):
        b = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.015,rounding_size=0.02",
            linewidth=1.6,
            edgecolor=ec,
            facecolor=fc,
        )
        b.set_path_effects([
            pe.SimplePatchShadow(offset=(1.2, -1.2), shadow_rgbFace=(0, 0, 0), alpha=0.10),
            pe.Normal(),
        ])
        ax_.add_patch(b)
        ax_.text(x + w / 2, y + h / 2, text, ha="center", va="center",
                 fontsize=fs, fontweight=fw, color=THEME["text"])

    cols = ["维度", "备选 A", "备选 B", "本文选型"]
    rows = [
        ("控制器", "ESP32", "STM32F103", "STM32F103RCT6"),
        ("调度", "裸机超循环", "FreeRTOS", "FreeRTOS"),
        ("通信", "HTTP 轮询", "MQTT", "MQTT"),
        ("安全", "无/弱加密", "AES + TLS", "AES-128 + TLS"),
    ]

    x0, y0, w, h = 0.04, 0.10, 0.92, 0.76
    colw = [0.18, 0.26, 0.26, 0.22]
    xs = [x0]
    for cw in colw:
        xs.append(xs[-1] + w * cw)

    header_h = h * 0.20
    row_h = (h - header_h) / len(rows)

    # header
    for i, c in enumerate(cols):
        box(ax, xs[i], y0 + h - header_h, xs[i + 1] - xs[i], header_h,
            c, THEME["fill_main"], THEME["edge"], fs=13, fw="bold")

    # body
    for r, row in enumerate(rows):
        y = y0 + h - header_h - (r + 1) * row_h
        for i, cell in enumerate(row):
            fc = THEME["fill_sub"] if i < 3 else "#E6F4EA"
            fw = "bold" if i == 3 else "normal"
            box(ax, xs[i], y, xs[i + 1] - xs[i], row_h, cell, fc, THEME["edge_soft"], fs=13, fw=fw)

    fig.tight_layout()
    fig.savefig(path, dpi=320)
    plt.close(fig)


def workflow(path):
    fig, ax = plt.subplots(figsize=(8.6, 12.2))
    setup_canvas(ax, "系统工作流程")

    # 主干流程（居中对齐，提升可读性）
    add_box(ax, 0.32, 0.90, 0.36, 0.055, "上电启动", fc=THEME["fill_main"], ec=THEME["edge"], fs=12)
    add_box(ax, 0.32, 0.81, 0.36, 0.055, "硬件初始化", fc=THEME["fill_main"], ec=THEME["edge"], fs=12)
    add_diamond(ax, 0.50, 0.70, 0.38, 0.12, "运行模式选择\n（居家 / 布防 / 应急）", fc=THEME["fill_warn"], ec="#C67C2F", fs=11)
    add_box(ax, 0.32, 0.58, 0.36, 0.055, "多源数据采集\n（指纹 / PIR / DHT / MQ-2 / 摄像头）", fc=THEME["fill_main"], ec=THEME["edge"], fs=11)
    add_diamond(ax, 0.50, 0.46, 0.36, 0.12, "事件判定\n（正常 / 异常）", fc=THEME["fill_warn"], ec="#C67C2F", fs=11)

    # 左支：正常路径（减少交叉，直下回环）
    add_box(ax, 0.08, 0.33, 0.26, 0.055, "状态上报", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)
    add_box(ax, 0.08, 0.24, 0.26, 0.055, "循环监测", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)

    # 右支：异常路径（垂直栈式排列）
    add_box(ax, 0.66, 0.36, 0.26, 0.055, "联动执行\n（门锁 / 蜂鸣器 / 燃气阀）", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)
    add_box(ax, 0.66, 0.28, 0.26, 0.055, "告警推送", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)
    add_box(ax, 0.66, 0.20, 0.26, 0.055, "日志记录", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)
    add_box(ax, 0.66, 0.12, 0.26, 0.055, "状态上报", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)
    add_box(ax, 0.66, 0.04, 0.26, 0.055, "循环监测", fc=THEME["fill_sub"], ec=THEME["edge_soft"], fs=11)

    # 主干箭头
    arrow(ax, 0.50, 0.90, 0.50, 0.865)
    arrow(ax, 0.50, 0.81, 0.50, 0.765)
    arrow(ax, 0.50, 0.64, 0.50, 0.607)
    arrow(ax, 0.50, 0.58, 0.50, 0.52)

    # 分支箭头（避免交叉，使用更短的水平连接）
    arrow(ax, 0.32, 0.46, 0.21, 0.385, "正常", fs=10)
    arrow(ax, 0.21, 0.33, 0.21, 0.295)

    arrow(ax, 0.68, 0.46, 0.79, 0.415, "异常", fs=10)
    arrow(ax, 0.79, 0.36, 0.79, 0.335)
    arrow(ax, 0.79, 0.28, 0.79, 0.255)
    arrow(ax, 0.79, 0.20, 0.79, 0.175)
    arrow(ax, 0.79, 0.12, 0.79, 0.095)

    # 回环：用弧线替代对角线，避免穿过图形
    ax.annotate(
        "",
        xy=(0.50, 0.74),
        xytext=(0.21, 0.24),
        arrowprops=dict(arrowstyle="-|>", lw=1.6, color=THEME["arrow"], mutation_scale=12, connectionstyle="arc3,rad=0.28"),
    )
    ax.text(0.30, 0.58, "继续监测", ha="center", va="center", fontsize=10, color="#3A4A5A",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.9))

    ax.annotate(
        "",
        xy=(0.50, 0.74),
        xytext=(0.79, 0.04),
        arrowprops=dict(arrowstyle="-|>", lw=1.6, color=THEME["arrow"], mutation_scale=12, connectionstyle="arc3,rad=-0.22"),
    )
    ax.text(0.68, 0.60, "恢复监测", ha="center", va="center", fontsize=10, color="#3A4A5A",
            bbox=dict(boxstyle="round,pad=0.12", fc="white", ec="none", alpha=0.9))

    fig.tight_layout()
    fig.savefig(path, dpi=300)
    plt.close(fig)


def state_machine(path):
    fig, ax = plt.subplots(figsize=(10, 7))
    setup_canvas(ax, "门禁与安防核心状态机")

    add_box(ax, 0.05, 0.45, 0.18, 0.10, "空闲监测")
    add_box(ax, 0.30, 0.68, 0.18, 0.10, "身份验证")
    add_box(ax, 0.30, 0.22, 0.18, 0.10, "布防监测")
    add_box(ax, 0.56, 0.68, 0.18, 0.10, "远程协同控制")
    add_box(ax, 0.56, 0.45, 0.18, 0.10, "告警触发")
    add_box(ax, 0.56, 0.22, 0.18, 0.10, "应急处置")
    add_box(ax, 0.80, 0.45, 0.15, 0.10, "恢复待机")

    arrow(ax, 0.23, 0.50, 0.30, 0.73, "指纹触发")
    arrow(ax, 0.39, 0.68, 0.13, 0.55, "验证通过")
    arrow(ax, 0.39, 0.68, 0.39, 0.32, "验证失败")
    arrow(ax, 0.48, 0.27, 0.56, 0.50, "PIR触发")
    arrow(ax, 0.48, 0.27, 0.56, 0.27, "气体超阈值")
    arrow(ax, 0.74, 0.73, 0.64, 0.55, "下发命令")
    arrow(ax, 0.74, 0.27, 0.74, 0.50, "升级告警")
    arrow(ax, 0.74, 0.50, 0.80, 0.50, "用户解除")
    arrow(ax, 0.95, 0.50, 0.23, 0.50, "网络恢复/超时重试")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def timing(path):
    fig, ax = plt.subplots(figsize=(11, 7))
    setup_canvas(ax, "设备与云端通信时序图")

    xs = [0.10, 0.35, 0.60, 0.85]
    labels = ["STM32主控", "ESP8266", "MQTT云平台", "移动端"]
    for x, lb in zip(xs, labels):
        ax.text(x, 0.93, lb, ha="center", va="center", fontsize=11, fontweight="bold", color=THEME["text"])
        ax.plot([x, x], [0.10, 0.90], linestyle="--", linewidth=1.0, color="#8AA3BF")

    y = 0.84
    step = 0.07
    msgs = [
        (0, 1, "设备状态采集/串口JSON封装"),
        (1, 2, "MQTT发布(状态上报)"),
        (2, 3, "云端转发与页面展示"),
        (3, 2, "用户下发控制命令"),
        (2, 1, "云端下发控制主题"),
        (1, 0, "串口转发控制命令"),
        (0, 1, "执行联动并回传结果"),
        (1, 2, "结果上报"),
        (2, 3, "状态同步完成"),
    ]
    for s, t, txt in msgs:
        ax.annotate(
            "",
            xy=(xs[t], y),
            xytext=(xs[s], y),
            arrowprops=dict(arrowstyle="-|>", lw=1.7, color=THEME["edge"], mutation_scale=12),
        )
        ax.text(
            (xs[s] + xs[t]) / 2,
            y + 0.015,
            txt,
            ha="center",
            va="bottom",
            fontsize=9,
            color="#2F3E4E",
            bbox=dict(boxstyle="round,pad=0.10", fc="white", ec="none", alpha=0.9),
        )
        y -= step

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def function_flow(path):
    fig, ax = plt.subplots(figsize=(8.5, 11))
    setup_canvas(ax, "系统功能流程图")

    add_box(ax, 0.32, 0.90, 0.36, 0.055, "系统启动与模式加载")
    add_box(ax, 0.32, 0.82, 0.36, 0.055, "多源数据采集（指纹/PIR/DHT/MQ-2）")
    add_diamond(ax, 0.50, 0.72, 0.34, 0.10, "触发条件判断\n(门禁/入侵/气体异常)")
    add_box(ax, 0.10, 0.62, 0.32, 0.055, "门禁流程\n身份验证与开锁控制")
    add_box(ax, 0.58, 0.62, 0.32, 0.055, "安防流程\n告警触发与联动执行")
    add_box(ax, 0.10, 0.52, 0.32, 0.055, "本地执行\n蜂鸣器/继电器/燃气阀")
    add_box(ax, 0.58, 0.52, 0.32, 0.055, "远程协同\nMQTT上报与命令接收")
    add_diamond(ax, 0.50, 0.40, 0.30, 0.09, "状态是否恢复")
    add_box(ax, 0.14, 0.30, 0.28, 0.055, "日志记录与事件存档")
    add_box(ax, 0.58, 0.30, 0.28, 0.055, "策略更新与参数调整")
    add_box(ax, 0.32, 0.18, 0.36, 0.055, "返回循环监测")

    arrow(ax, 0.50, 0.90, 0.50, 0.875)
    arrow(ax, 0.50, 0.82, 0.50, 0.77)
    arrow(ax, 0.37, 0.72, 0.26, 0.675, "门禁事件")
    arrow(ax, 0.63, 0.72, 0.74, 0.675, "安防/应急事件")
    arrow(ax, 0.26, 0.62, 0.26, 0.575)
    arrow(ax, 0.74, 0.62, 0.74, 0.575)
    arrow(ax, 0.26, 0.52, 0.43, 0.425)
    arrow(ax, 0.74, 0.52, 0.57, 0.425)
    arrow(ax, 0.43, 0.40, 0.28, 0.355, "否")
    arrow(ax, 0.57, 0.40, 0.72, 0.355, "是")
    arrow(ax, 0.28, 0.30, 0.50, 0.235)
    arrow(ax, 0.72, 0.30, 0.50, 0.235)
    arrow(ax, 0.50, 0.18, 0.50, 0.86, "持续运行")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def uart_dataflow(path):
    fig, ax = plt.subplots(figsize=(10, 6.5))
    setup_canvas(ax, "串口与外设数据流向图")

    add_box(ax, 0.40, 0.73, 0.20, 0.10, "STM32主控", fc="#DDEBFF")
    add_box(ax, 0.08, 0.73, 0.22, 0.10, "AS608/调试串口\n(USART1)", fc="#EEF5FF")
    add_box(ax, 0.70, 0.73, 0.22, 0.10, "ESP8266联网\n(USART3)", fc="#EEF5FF")
    add_box(ax, 0.08, 0.46, 0.22, 0.10, "传感器组\nPIR/DHT/MQ-2", fc="#F5F8FF")
    add_box(ax, 0.40, 0.46, 0.20, 0.10, "执行器组\n继电器/蜂鸣器", fc="#F5F8FF")
    add_box(ax, 0.70, 0.46, 0.22, 0.10, "MQTT云平台", fc="#F5F8FF")
    add_box(ax, 0.70, 0.20, 0.22, 0.10, "移动端客户端", fc="#F5F8FF")

    arrow(ax, 0.30, 0.78, 0.40, 0.78, "身份数据/调试命令")
    arrow(ax, 0.60, 0.78, 0.70, 0.78, "状态上报/控制命令")
    arrow(ax, 0.19, 0.56, 0.45, 0.73, "采样数据")
    arrow(ax, 0.50, 0.73, 0.50, 0.56, "本地联动控制")
    arrow(ax, 0.81, 0.73, 0.81, 0.56, "MQTT发布/订阅")
    arrow(ax, 0.81, 0.46, 0.81, 0.30, "消息转发")
    arrow(ax, 0.81, 0.20, 0.81, 0.46, "远程指令")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def task_gantt(path):
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.set_facecolor("#FFFFFF")
    ax.set_title("任务调度时序甘特图", fontsize=17, fontweight="bold", color=THEME["text"], pad=12)
    tasks = [
        ("Task_Security_Alarm", [(0.0, 0.7), (2.0, 0.6), (4.0, 0.7)], "#E15759"),
        ("Task_Wifi_MQTT", [(0.6, 1.2), (2.8, 1.0), (5.0, 1.0)], "#4E79A7"),
        ("Task_Fingerprint", [(0.4, 0.6), (3.2, 0.7), (5.6, 0.6)], "#59A14F"),
        ("Task_Sensor", [(1.0, 0.8), (3.8, 0.8), (6.0, 0.8)], "#F28E2B"),
        ("Task_Capture", [(2.4, 0.9), (5.2, 0.8)], "#B07AA1"),
        ("Task_SystemMonitor", [(1.8, 0.5), (4.6, 0.5), (6.8, 0.5)], "#76B7B2"),
    ]
    y = list(range(len(tasks)))
    ax.set_yticks(y)
    ax.set_yticklabels([t[0] for t in tasks], fontsize=9)
    for i, (_, segs, color) in enumerate(tasks):
        for st, du in segs:
            ax.broken_barh([(st, du)], (i - 0.35, 0.7), facecolors=color, edgecolors="black", linewidth=0.4)

    ax.set_xlim(0, 8)
    ax.set_xlabel("时间片 (s)")
    ax.grid(axis="x", linestyle="--", alpha=0.30, color="#8AA3BF")
    ax.set_ylim(-0.8, len(tasks) - 0.2)
    ax.invert_yaxis()
    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def fault_recovery(path):
    fig, ax = plt.subplots(figsize=(8.5, 10.5))
    setup_canvas(ax, "异常处理与恢复流程图")

    add_box(ax, 0.33, 0.90, 0.34, 0.055, "系统运行中")
    add_diamond(ax, 0.50, 0.80, 0.30, 0.10, "检测到异常？")
    add_box(ax, 0.10, 0.70, 0.30, 0.055, "继续常规监测")
    add_box(ax, 0.60, 0.70, 0.30, 0.055, "异常分类\n(通信/传感/执行)")
    add_box(ax, 0.60, 0.60, 0.30, 0.055, "触发降级策略")
    add_box(ax, 0.60, 0.50, 0.30, 0.055, "记录日志并上报")
    add_diamond(ax, 0.50, 0.38, 0.30, 0.10, "恢复条件满足？")
    add_box(ax, 0.10, 0.28, 0.30, 0.055, "维持降级并重试")
    add_box(ax, 0.60, 0.28, 0.30, 0.055, "恢复正常模式")
    add_box(ax, 0.33, 0.16, 0.34, 0.055, "返回循环监测")

    arrow(ax, 0.50, 0.90, 0.50, 0.85)
    arrow(ax, 0.35, 0.80, 0.25, 0.755, "否")
    arrow(ax, 0.65, 0.80, 0.75, 0.755, "是")
    arrow(ax, 0.75, 0.70, 0.75, 0.655)
    arrow(ax, 0.75, 0.60, 0.75, 0.555)
    arrow(ax, 0.75, 0.50, 0.56, 0.43)
    arrow(ax, 0.44, 0.38, 0.25, 0.335, "否")
    arrow(ax, 0.56, 0.38, 0.75, 0.335, "是")
    arrow(ax, 0.25, 0.28, 0.50, 0.215)
    arrow(ax, 0.75, 0.28, 0.50, 0.215)
    arrow(ax, 0.25, 0.70, 0.50, 0.905, "持续运行")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def remote_control_flow(path):
    fig, ax = plt.subplots(figsize=(8.8, 10.2))
    setup_canvas(ax, "远程协同控制闭环流程图")

    add_box(ax, 0.30, 0.90, 0.40, 0.055, "移动端发起控制请求")
    add_box(ax, 0.30, 0.81, 0.40, 0.055, "云平台接收并转发命令")
    add_box(ax, 0.30, 0.72, 0.40, 0.055, "ESP8266下发至STM32")
    add_diamond(ax, 0.50, 0.61, 0.34, 0.10, "本地安全校验通过？")
    add_box(ax, 0.08, 0.50, 0.34, 0.055, "拒绝执行并返回错误码")
    add_box(ax, 0.58, 0.50, 0.34, 0.055, "执行联动动作")
    add_box(ax, 0.58, 0.41, 0.34, 0.055, "更新状态并记录日志")
    add_box(ax, 0.30, 0.30, 0.40, 0.055, "执行结果回传云平台")
    add_box(ax, 0.30, 0.21, 0.40, 0.055, "移动端展示最新状态")
    add_box(ax, 0.30, 0.12, 0.40, 0.055, "进入下一轮监听")

    arrow(ax, 0.50, 0.90, 0.50, 0.865)
    arrow(ax, 0.50, 0.81, 0.50, 0.775)
    arrow(ax, 0.50, 0.72, 0.50, 0.66)
    arrow(ax, 0.40, 0.61, 0.25, 0.555, "否")
    arrow(ax, 0.60, 0.61, 0.75, 0.555, "是")
    arrow(ax, 0.75, 0.50, 0.75, 0.465)
    arrow(ax, 0.75, 0.41, 0.58, 0.355)
    arrow(ax, 0.25, 0.50, 0.42, 0.355)
    arrow(ax, 0.50, 0.30, 0.50, 0.265)
    arrow(ax, 0.50, 0.21, 0.50, 0.175)
    arrow(ax, 0.50, 0.12, 0.50, 0.885, "闭环控制")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def stability_flow(path):
    fig, ax = plt.subplots(figsize=(8.5, 10))
    setup_canvas(ax, "72h稳定性测试流程图")

    add_box(ax, 0.30, 0.90, 0.40, 0.055, "测试准备与参数初始化")
    add_box(ax, 0.30, 0.81, 0.40, 0.055, "启动72h连续运行")
    add_box(ax, 0.30, 0.72, 0.40, 0.055, "周期采样状态与日志")
    add_diamond(ax, 0.50, 0.61, 0.30, 0.10, "检测异常事件？")
    add_box(ax, 0.08, 0.50, 0.34, 0.055, "记录常规数据\n(温湿度/告警/通信)")
    add_box(ax, 0.58, 0.50, 0.34, 0.055, "触发异常复测与归因")
    add_diamond(ax, 0.50, 0.38, 0.30, 0.10, "72h是否结束？")
    add_box(ax, 0.08, 0.27, 0.34, 0.055, "继续运行并采样")
    add_box(ax, 0.58, 0.27, 0.34, 0.055, "统计重启/阻塞/丢包")
    add_box(ax, 0.30, 0.15, 0.40, 0.055, "输出稳定性评估结论")

    arrow(ax, 0.50, 0.90, 0.50, 0.865)
    arrow(ax, 0.50, 0.81, 0.50, 0.775)
    arrow(ax, 0.50, 0.72, 0.50, 0.66)
    arrow(ax, 0.40, 0.61, 0.25, 0.555, "否")
    arrow(ax, 0.60, 0.61, 0.75, 0.555, "是")
    arrow(ax, 0.25, 0.50, 0.44, 0.43)
    arrow(ax, 0.75, 0.50, 0.56, 0.43)
    arrow(ax, 0.40, 0.38, 0.25, 0.325, "否")
    arrow(ax, 0.60, 0.38, 0.75, 0.325, "是")
    arrow(ax, 0.25, 0.27, 0.50, 0.745, "循环采样")
    arrow(ax, 0.75, 0.27, 0.50, 0.205)

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def future_arch(path):
    fig, ax = plt.subplots(figsize=(10, 6.5))
    setup_canvas(ax, "云边端扩展架构图")

    add_box(ax, 0.05, 0.58, 0.22, 0.12, "家庭终端层\nSTM32安防节点", fc="#EAF2FF")
    add_box(ax, 0.38, 0.58, 0.24, 0.12, "边缘网关层\n协议转换/本地规则", fc="#EEF5FF")
    add_box(ax, 0.73, 0.58, 0.22, 0.12, "云平台层\n数据服务/策略中心", fc="#EAF2FF")
    add_box(ax, 0.05, 0.28, 0.22, 0.10, "多协议设备\nZigBee/WiFi/BLE", fc="#F7FAFF")
    add_box(ax, 0.38, 0.28, 0.24, 0.10, "边缘智能模块\n异常判定/缓存", fc="#F7FAFF")
    add_box(ax, 0.73, 0.28, 0.22, 0.10, "应用层\n移动端/运维平台", fc="#F7FAFF")

    arrow(ax, 0.27, 0.64, 0.38, 0.64, "状态上报/控制下发")
    arrow(ax, 0.62, 0.64, 0.73, 0.64, "云边协同")
    arrow(ax, 0.16, 0.38, 0.16, 0.58, "设备接入")
    arrow(ax, 0.50, 0.38, 0.50, 0.58, "边缘计算")
    arrow(ax, 0.84, 0.58, 0.84, 0.38, "服务输出")
    arrow(ax, 0.84, 0.38, 0.62, 0.33, "策略回流")

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


if __name__ == "__main__":
    selection_matrix("images/fig2_3_selection_matrix.png")
    workflow("images/fig2_2_workflow.png")
    state_machine("images/fig4_2_state_machine.png")
    timing("images/fig4_3_comm_timing.png")
    function_flow("images/fig4_4_function_flow.png")
    uart_dataflow("images/fig3_6_uart_dataflow.png")
    task_gantt("images/fig4_5_task_gantt.png")
    fault_recovery("images/fig4_6_fault_recovery.png")
    remote_control_flow("images/fig5_5_remote_control_flow.png")
    stability_flow("images/fig6_4_stability_flow.png")
    future_arch("images/fig7_1_future_arch.png")
    print("Generated 10 flowchart/architecture images in images/.")

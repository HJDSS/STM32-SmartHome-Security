import matplotlib.pyplot as plt
import matplotlib.patches as patches


def main():
    fig, ax = plt.subplots(figsize=(12, 5.6), dpi=200)

    tasks = [
        ("安防告警任务 P5", 5),
        ("网络通信任务 P4", 4),
        ("指纹识别任务 P3", 3),
        ("传感采集任务 P3", 2),
        ("图像抓拍任务 P2", 1),
        ("系统监测任务 P1", 0),
    ]

    h = 0.72
    colors = {
        "alarm": "#E53935",
        "net": "#1E88E5",
        "finger": "#8E24AA",
        "sensor": "#43A047",
        "camera": "#FB8C00",
        "monitor": "#546E7A",
    }

    # periodic baseline
    bars = [
        (0.00, 0.25, 5, colors["alarm"], "周期扫描"),
        (0.40, 0.32, 4, colors["net"], "MQTT维护"),
        (0.10, 0.22, 3, colors["finger"], "识别流程"),
        (0.30, 0.25, 2, colors["sensor"], "采样+滤波"),
        (0.75, 0.32, 1, colors["camera"], "抓拍写卡"),
        (1.10, 0.25, 0, colors["monitor"], "状态上报"),
        (1.45, 0.20, 3, colors["finger"], "二次识别"),
        (1.75, 0.22, 4, colors["net"], "重连检测"),
        (2.00, 0.25, 2, colors["sensor"], "采样更新"),
        (2.40, 0.18, 0, colors["monitor"], "喂狗检测"),
        (2.70, 0.22, 4, colors["net"], "命令下发"),
    ]

    for s, d, y, c, txt in bars:
        ax.broken_barh([(s, d)], (y - h / 2, h), facecolors=c, edgecolors="black", linewidth=0.6)
        ax.text(s + d / 2, y, txt, ha="center", va="center", fontsize=8, color="white")

    # preemption segment: alarm interrupts lower-priority tasks
    preempt_start = 1.62
    preempt_dur = 0.20
    ax.broken_barh([(preempt_start, preempt_dur)], (5 - h / 2, h), facecolors=colors["alarm"], edgecolors="black", linewidth=0.8)
    ax.text(preempt_start + preempt_dur / 2, 5, "PIR触发抢占", ha="center", va="center", fontsize=8, color="white")

    # blocked windows (resource lock / I/O wait)
    blocked = [
        (0.82, 0.12, 4, "等待串口DMA"),
        (1.90, 0.10, 1, "等待SD写入"),
    ]
    for s, d, y, txt in blocked:
        rect = patches.Rectangle((s, y - h / 2), d, h, facecolor="#BDBDBD", edgecolor="black", hatch="///", linewidth=0.8)
        ax.add_patch(rect)
        ax.text(s + d / 2, y, txt, ha="center", va="center", fontsize=7)

    # arrows and annotations
    ax.annotate(
        "抢占点：安防告警任务(P5)中断唤醒后抢占P4/P3任务",
        xy=(preempt_start + 0.02, 5.35),
        xytext=(0.15, 5.95),
        arrowprops=dict(arrowstyle="->", lw=1.0),
        fontsize=8,
    )
    ax.annotate(
        "阻塞点：I/O等待，CPU可切换执行其它就绪任务",
        xy=(0.88, 4.2),
        xytext=(0.15, 4.85),
        arrowprops=dict(arrowstyle="->", lw=1.0),
        fontsize=8,
    )

    ax.set_xlim(0, 3.0)
    ax.set_ylim(-0.8, 6.3)
    ax.set_xlabel("时间 / s", fontsize=10)
    ax.set_yticks([t[1] for t in tasks])
    ax.set_yticklabels([t[0] for t in tasks], fontsize=9)
    ax.set_title("FreeRTOS任务调度甘特图（含优先级抢占与阻塞点）", fontsize=12)
    ax.grid(axis="x", linestyle="--", alpha=0.35)
    plt.tight_layout()

    out = r"D:\ziliao\毕业设计\quanbu\论文相关\源文件\images\fig4_5_task_gantt.png"
    plt.savefig(out, dpi=220)
    print(f"Saved: {out}")


if __name__ == "__main__":
    main()

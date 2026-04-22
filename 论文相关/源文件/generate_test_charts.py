import matplotlib.pyplot as plt

plt.rcParams["font.sans-serif"] = ["Microsoft YaHei", "SimHei", "SimSun"]
plt.rcParams["axes.unicode_minus"] = False


def accuracy_bar(path):
    labels = ["门禁识别准确率", "入侵检测误报率", "图像上传成功率"]
    values = [98.0, 3.2, 96.5]
    targets = [95.0, 5.0, 90.0]

    fig, ax = plt.subplots(figsize=(9, 5))
    x = range(len(labels))
    bars = ax.bar(x, values, width=0.5, color=["#4C78A8", "#F58518", "#54A24B"])
    ax.plot(x, targets, color="#D62728", marker="o", linewidth=2, label="目标阈值")
    ax.set_title("关键功能准确率统计", fontsize=14, fontweight="bold")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=0)
    ax.set_ylabel("百分比 (%)")
    ax.set_ylim(0, 110)
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.legend(loc="upper right")

    for b, v in zip(bars, values):
        ax.text(b.get_x() + b.get_width() / 2, v + 1.2, f"{v:.1f}", ha="center", va="bottom", fontsize=10)

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


def latency_line(path):
    labels = ["门禁联动", "入侵报警", "燃气应急", "远程控制"]
    values = [1.20, 1.35, 1.10, 1.90]  # s
    target = 2.50

    fig, ax = plt.subplots(figsize=(9, 5))
    x = range(len(labels))
    ax.plot(x, values, marker="o", linewidth=2.2, color="#1F77B4", label="实测平均时延")
    ax.hlines(target, xmin=0, xmax=len(labels) - 1, colors="#D62728", linestyles="--", linewidth=2, label="目标上限")
    ax.set_title("关键路径响应时延对比", fontsize=14, fontweight="bold")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels)
    ax.set_ylabel("时延 (s)")
    ax.set_ylim(0, 3.0)
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.legend(loc="upper left")

    for xi, yi in zip(x, values):
        ax.text(xi, yi + 0.06, f"{yi:.2f}", ha="center", va="bottom", fontsize=10)

    fig.tight_layout()
    fig.savefig(path, dpi=220)
    plt.close(fig)


if __name__ == "__main__":
    accuracy_bar("images/fig6_2_accuracy_bar.png")
    latency_line("images/fig6_3_latency_line.png")
    print("Generated fig6_2 and fig6_3 in images/.")

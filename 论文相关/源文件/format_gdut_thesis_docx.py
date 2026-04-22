# -*- coding: utf-8 -*-
"""
将 Word 论文初稿批量调整为《广东工业大学本科生毕业设计（论文）格式规范》
（2026 届毕业设计手册附件 3-26「格式规范」）中的主要版式要求。

依据手册「第三条 毕业设计（论文）的撰写规范」抽取并程序化实现：
  - 页面：A4；页边距上 30mm、下 25mm、左 30mm、右 20mm；正文 1.5 倍行距。
  - 字体字号：章标题三号黑体加粗；节标题小四黑体加粗；条标题小四黑体；
    正文小四宋体；西文与数字 Times New Roman；节/条段前段后约 0.5 行（按 12pt×1.5 行距折算）。
  - 表内文字五号宋体；表题「表 x.x」段落按五号黑体加粗（数字字母 TNR）；
    图题「图 x.x」段落五号宋体。

未自动处理（需在 Word 中人工或使用分节符完成）：
  - 封面、中英文摘要、目录不编页码，绪论起阿拉伯数字页脚右侧至附录——需分节与「链接到前一节」设置。
  - 公式编辑器版本、参考文献 GB/T 7714 域、自动目录样式与学院细则。

完整命令行说明（含参数示例）请执行：
  python format_gdut_thesis_docx.py -h

依赖：  pip install python-docx
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Mm, Pt
from docx.table import Table


# --- 手册字号（pt）---
PT_SAN_HAO = 16  # 三号 — 章标题、「摘要」标题
PT_SI_HAO = 14  # 四号 — 「关键词」三字
PT_XIAO_SI = 12  # 小四 — 正文、节/条标题（节/条为黑体）
PT_WU_HAO = 10.5  # 五号 — 表题、图题、表内字

LINE_SPACING_MULT = 1.5
# 「节」「条」段前段后各 0.5 行：按正文小四 + 1.5 倍行距近似
HALF_LINE_BEFORE_AFTER = Pt(round(PT_XIAO_SI * LINE_SPACING_MULT * 0.5))

RE_CHAPTER_ZH = re.compile(r"^第[0-9一二三四五六七八九十百千]+章")
RE_CHAPTER_NUM = re.compile(r"^\d+\s+")
RE_SECTION = re.compile(r"^\d+\.\d+(?:\.\d+)?\s+")
RE_SUBSECTION = re.compile(r"^\d+\.\d+\.\d+\s+")
RE_FIG_CAPTION = re.compile(r"^图\s*\d+\.\d+")
RE_TAB_CAPTION = re.compile(r"^表\s*\d+\.\d+")
RE_KEYWORDS_PREFIX_ZH = re.compile(r"^(关键词\s*[:：])", re.UNICODE)
RE_KEYWORDS_PREFIX_EN = re.compile(r"^(Key\s*words\s*[:：])", re.I | re.UNICODE)


def _paragraph_clear_runs(p) -> None:
    """保留段落属性(pPr)，删除所有 w:r，便于重建 runs。"""
    p_elm = p._element
    for child in list(p_elm):
        if child.tag == qn("w:pPr"):
            continue
        p_elm.remove(child)


def set_run_font(
    run,
    *,
    east_asia: str,
    latin: str,
    size_pt: float,
    bold: bool | None = None,
) -> None:
    run.font.name = latin
    r = run._element.get_or_add_rPr()
    rFonts = r.find(qn("w:rFonts"))
    if rFonts is None:
        rFonts = OxmlElement("w:rFonts")
        r.insert(0, rFonts)
    rFonts.set(qn("w:eastAsia"), east_asia)
    rFonts.set(qn("w:ascii"), latin)
    rFonts.set(qn("w:hAnsi"), latin)
    run.font.size = Pt(size_pt)
    if bold is not None:
        run.bold = bold


def classify_paragraph(text: str, style_name: str) -> str:
    t = text.strip()
    if not t:
        return "empty"
    sn = (style_name or "").lower()
    if _is_toc_entry_style(style_name or ""):
        return "toc_entry"
    if "heading 1" in sn or sn.startswith("标题 1"):
        return "chapter"
    if "heading 2" in sn or sn.startswith("标题 2"):
        return "section"
    if "heading 3" in sn or sn.startswith("标题 3"):
        return "article"
    if t in ("摘要",) or t.upper() in ("ABSTRACT",):
        return "abstract_title"
    if t == "目录" or re.match(r"^contents$", t, re.I):
        return "toc_title"
    if t.startswith("关键词") or re.match(r"^key\s*words", t, re.I):
        return "keywords"
    if t.startswith("参考文献"):
        return "refs_title"
    if t.startswith("致谢"):
        return "ack_title"
    if re.match(r"^附录\s*[A-ZＡ-Ｚa-z]", t):
        return "appendix_title"
    if RE_TAB_CAPTION.match(t):
        return "table_caption"
    if RE_FIG_CAPTION.match(t):
        return "figure_caption"
    if RE_SUBSECTION.match(t):
        return "article"
    if RE_SECTION.match(t):
        # 1.1 节 vs 1.1.1 条：已由 subsection 优先
        parts = t.split(None, 1)[0].split(".")
        if len(parts) >= 3 and all(p.isdigit() for p in parts):
            return "article"
        return "section"
    if RE_CHAPTER_ZH.match(t) or (RE_CHAPTER_NUM.match(t) and not re.match(r"^\d+\.\d+", t)):
        return "chapter"
    return "body"


def _is_toc_entry_style(style_name: str) -> bool:
    """自动目录域生成的段落样式（中/英文 Word 常见命名）。"""
    s = (style_name or "").strip()
    sl = s.lower()
    if sl.startswith("toc ") and not sl.startswith("toc heading"):
        return True
    if re.match(r"^目录\s*\d", s):
        return True
    return False


def _format_keywords_split(p) -> bool:
    """
    按手册：「关键词」三字四号黑体加粗；关键词内容小四宋体。
    若整段可解析为「关键词：」+ 正文 或「Key words:」+ 正文，则重写 runs 并返回 True。
    """
    t = p.text.strip()
    m = RE_KEYWORDS_PREFIX_ZH.match(t)
    if m:
        suffix = t[m.end() :]
        _paragraph_clear_runs(p)
        r0 = p.add_run(m.group(1))
        set_run_font(r0, east_asia="黑体", latin="Times New Roman", size_pt=PT_SI_HAO, bold=True)
        r1 = p.add_run(suffix)
        set_run_font(r1, east_asia="宋体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
        return True
    m = RE_KEYWORDS_PREFIX_EN.match(t)
    if m:
        suffix = t[m.end() :]
        _paragraph_clear_runs(p)
        r0 = p.add_run(m.group(1))
        set_run_font(r0, east_asia="Times New Roman", latin="Times New Roman", size_pt=PT_SI_HAO, bold=True)
        r1 = p.add_run(suffix)
        set_run_font(r1, east_asia="宋体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
        return True
    return False


def format_paragraph(p, kind: str) -> None:
    pf = p.paragraph_format
    pf.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
    pf.line_spacing = LINE_SPACING_MULT
    pf.first_line_indent = None

    if kind == "empty":
        return

    if kind == "chapter":
        pf.space_before = HALF_LINE_BEFORE_AFTER
        pf.space_after = HALF_LINE_BEFORE_AFTER
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_SAN_HAO, bold=True)
        p.paragraph_format.first_line_indent = Pt(0)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        return

    if kind == "section":
        pf.space_before = HALF_LINE_BEFORE_AFTER
        pf.space_after = HALF_LINE_BEFORE_AFTER
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=True)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        return

    if kind == "article":
        pf.space_before = HALF_LINE_BEFORE_AFTER
        pf.space_after = HALF_LINE_BEFORE_AFTER
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        return

    if kind == "abstract_title":
        pf.space_before = Pt(0)
        pf.space_after = Pt(12)
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_SAN_HAO, bold=True)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        return

    if kind == "toc_title":
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=True)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        return

    if kind == "keywords":
        if not _format_keywords_split(p):
            for r in p.runs:
                set_run_font(r, east_asia="宋体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        return

    if kind in ("refs_title", "ack_title", "appendix_title"):
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_SAN_HAO, bold=True)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        return

    if kind == "toc_entry":
        pf.space_before = Pt(0)
        pf.space_after = Pt(0)
        pf.first_line_indent = Pt(0)
        for r in p.runs:
            set_run_font(r, east_asia="宋体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        return

    if kind == "table_caption":
        for r in p.runs:
            set_run_font(r, east_asia="黑体", latin="Times New Roman", size_pt=PT_WU_HAO, bold=True)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        return

    if kind == "figure_caption":
        for r in p.runs:
            set_run_font(r, east_asia="宋体", latin="Times New Roman", size_pt=PT_WU_HAO, bold=False)
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        return

    # body
    pf.space_before = Pt(0)
    pf.space_after = Pt(0)
    pf.first_line_indent = Pt(2 * 12)  # 约两个汉字，手册「每段起行空两格」常用实现
    for r in p.runs:
        set_run_font(r, east_asia="宋体", latin="Times New Roman", size_pt=PT_XIAO_SI, bold=False)
    # 不修改 p.alignment，保留用户两端对齐等设置


def format_table(table: Table) -> None:
    for row in table.rows:
        for cell in row.cells:
            for p in cell.paragraphs:
                p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
                p.paragraph_format.line_spacing = LINE_SPACING_MULT
                for r in p.runs:
                    set_run_font(r, east_asia="宋体", latin="Times New Roman", size_pt=PT_WU_HAO, bold=False)
            for nested in cell.tables:
                format_table(nested)


def apply_section_margins(document: Document) -> None:
    for sec in document.sections:
        sec.top_margin = Mm(30)
        sec.bottom_margin = Mm(25)
        sec.left_margin = Mm(30)
        sec.right_margin = Mm(20)


def apply_normal_style(document: Document) -> None:
    """将内置「正文」样式设为与手册一致的小四宋体 + 西文 TNR，便于后续新输入文字。"""
    try:
        st = document.styles["Normal"]
    except KeyError:
        return
    try:
        st.font.name = "Times New Roman"
        st.font.size = Pt(PT_XIAO_SI)
        r_pr = st.element.get_or_add_rPr()
        r_fonts = r_pr.find(qn("w:rFonts"))
        if r_fonts is None:
            r_fonts = OxmlElement("w:rFonts")
            r_pr.insert(0, r_fonts)
        r_fonts.set(qn("w:eastAsia"), "宋体")
        r_fonts.set(qn("w:ascii"), "Times New Roman")
        r_fonts.set(qn("w:hAnsi"), "Times New Roman")
    except (AttributeError, ValueError, TypeError):
        pass


PT_XIAO_WU = 9  # 小五 — 页眉页脚（近似手册页码字号）


def format_headers_footers(document: Document) -> None:
    """统一页眉页脚中已有文字为小五、1.5 倍行距（不插入页码域）。"""

    def _walk(paragraphs) -> None:
        for p in paragraphs:
            p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
            p.paragraph_format.line_spacing = LINE_SPACING_MULT
            for r in p.runs:
                set_run_font(
                    r,
                    east_asia="宋体",
                    latin="Times New Roman",
                    size_pt=PT_XIAO_WU,
                    bold=None,
                )

    for sec in document.sections:
        _walk(sec.header.paragraphs)
        _walk(sec.footer.paragraphs)
        if sec.different_first_page_header_footer:
            _walk(sec.first_page_header.paragraphs)
            _walk(sec.first_page_footer.paragraphs)


def format_document(
    document: Document,
    *,
    apply_margins: bool = True,
    format_tables: bool = True,
    headers_footers: bool = False,
    patch_normal: bool = True,
    verbose: bool = False,
) -> dict[str, int]:
    counts: dict[str, int] = {}
    if apply_margins:
        apply_section_margins(document)
    if patch_normal:
        apply_normal_style(document)
    if headers_footers:
        format_headers_footers(document)
    for p in document.paragraphs:
        kind = classify_paragraph(p.text, p.style.name if p.style else "")
        counts[kind] = counts.get(kind, 0) + 1
        format_paragraph(p, kind)
    if format_tables:
        for table in document.tables:
            format_table(table)
    if verbose and counts:
        parts = ", ".join(f"{k}={v}" for k, v in sorted(counts.items(), key=lambda x: -x[1]))
        print("段落分类统计:", parts, file=sys.stderr)
    return counts


def main(argv: list[str]) -> int:
    epilog = """
使用说明（建议先备份论文再运行）：
  1) 安装依赖：
       pip install python-docx
  2) 在「终端 / PowerShell」进入脚本所在目录，或写全脚本路径。
  3) 基本用法（不覆盖原稿，同目录生成 原名_gdut_fmt.docx）：
       python format_gdut_thesis_docx.py  我的论文.docx
  4) 指定输出文件：
       python format_gdut_thesis_docx.py  我的论文.docx  -o  我的论文_已排版.docx
  5) 若要让输出路径与输入为同一文件（原地覆盖），必须加 --inplace：
       python format_gdut_thesis_docx.py  我的论文.docx  -o  我的论文.docx  --inplace
  6) 其它常用参数：
       --verbose           在终端打印各类型段落数量统计
       --headers-footers   顺带统一页眉页脚字号（小五）与行距
       --no-margins        不修改页边距（只改字体段落等）
       --skip-tables       不处理表格内文字
       --no-normal-style   不修改「正文 Normal」样式
       --backup            若输出文件已存在，先复制一份 .docx.bak 再覆盖

注意：脚本不会插入分节符与页码域；封面/摘要/目录与正文页码分节请在 Word 中按手册完成。
"""
    parser = argparse.ArgumentParser(
        description="按广东工业大学 2026 届毕业设计（论文）手册「格式规范」调整 docx 版式。",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilog,
    )
    parser.add_argument("input", type=Path, help="输入 .docx 路径")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="输出 .docx；默认在输入同目录生成 *_gdut_fmt.docx",
    )
    parser.add_argument(
        "--inplace",
        action="store_true",
        help="允许输出路径与输入为同一文件（否则拒绝覆盖原稿）",
    )
    parser.add_argument(
        "--backup",
        action="store_true",
        help="若输出覆盖已存在文件，先把原输出文件复制为 .docx.bak",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="在 stderr 打印段落类型统计",
    )
    parser.add_argument(
        "--headers-footers",
        action="store_true",
        help="同时格式化页眉页脚中的文字",
    )
    parser.add_argument(
        "--no-margins",
        action="store_true",
        help="不修改各节页边距",
    )
    parser.add_argument(
        "--skip-tables",
        action="store_true",
        help="不修改表格内字体与行距",
    )
    parser.add_argument(
        "--no-normal-style",
        action="store_true",
        help="不修改内置「正文 Normal」样式",
    )
    args = parser.parse_args(argv)

    src: Path = args.input.expanduser().resolve()
    if not src.is_file():
        print(f"找不到文件: {src}", file=sys.stderr)
        return 2

    if args.output:
        out: Path = args.output.expanduser().resolve()
    else:
        out = src.with_name(src.stem + "_gdut_fmt.docx")
    if out == src and not args.inplace:
        print(
            "输出与输入为同一文件。若确需覆盖原稿，请追加 --inplace；否则请使用 -o 指定另一输出路径。",
            file=sys.stderr,
        )
        return 3

    if args.backup and out.exists():
        shutil.copy2(out, str(out) + ".bak")

    doc = Document(str(src))
    format_document(
        doc,
        apply_margins=not args.no_margins,
        format_tables=not args.skip_tables,
        headers_footers=args.headers_footers,
        patch_normal=not args.no_normal_style,
        verbose=args.verbose,
    )
    out.parent.mkdir(parents=True, exist_ok=True)
    doc.save(str(out))
    print(f"已写入: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

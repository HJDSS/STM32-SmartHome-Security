#ifndef OLED_UI_STR_H
#define OLED_UI_STR_H

/* 128x64 / 8x16：一行固定 16 个半角，须与 OLED_ShowString(...,16) 完全一致。
 * 勿用行首空格“顶格”：首列被挡/列偏移时整格丢失，会表现为首字母错成别的字符、行尾被截断。
 * 若仍偏左/偏右，请改 board_config.h 的 OLED_COLUMN_OFFSET（多為 SH1106 试 2）或 OLED_GRAM_X_SHIFT。 */
#define OLED_S16_SP16     "                "
#define OLED_S16_SP15     "               "
#define OLED_S16_INPWD    "Input Password  "
#define OLED_S16_PWDACC   "Password Access "
#define OLED_S16_PRESS    "Press # Confirm "
#define OLED_S16_SEC_ARM  "SEC: ARMED      "
#define OLED_S16_SEC_DIS  "SEC: DISARM     "
#define OLED_S16_ALM_TOP  "SECURITY ALARM  "
#define OLED_S16_INTRU    "INTRUSION ALARM "
#define OLED_S16_PIR      "PIR DETECTED    "
#define OLED_S16_UNLOCK   "UNLOCK TO CLEAR "

#endif

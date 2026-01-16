#include "../include/kernel.h"
#include "../include/string.h"
#include <stdint.h>
#include <time.h>
#include "../include/apps.h"

// Provided by your window manager / app manager
extern int open_app_count;
extern char* open_apps[];

// Provided by your app launcher system
extern void launch_app(AppDefinition* app);

// Provided by your kernel timer
extern uint64_t timer_ms(void);

// Provided by your libc or Tiny64 time module
extern time_t time(time_t* t);
extern struct tm* localtime(const time_t* t);
extern size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm);


#define UI_SHADOW_SOFT 0x20000000
#define UI_SHADOW_DARK 0x40000000

#define UI_BORDER_LIGHT 0xFFE7E7E7
#define UI_BORDER_MEDIUM 0xFFB5B5B5
#define UI_BORDER_DARK 0xFF6A6A6A

#define UI_TITLE_GRAD_TOP 0xFF4F7CCF
#define UI_TITLE_GRAD_BOTTOM 0xFF2B4A8A
#define UI_TITLE_ACTIVE_TOP 0xFF6FA8FF
#define UI_TITLE_ACTIVE_BOTTOM 0xFF3A6ED6

#define UI_BUTTON_TOP 0xFFFDFDFD
#define UI_BUTTON_BOTTOM 0xFFE1E1E1
#define UI_BUTTON_HOVER_TOP 0xFFFFFFFF
#define UI_BUTTON_HOVER_BOTTOM 0xFFEAEAEA

#define UI_DESKTOP_TOP 0xFF3A6EA5
#define UI_DESKTOP_BOTTOM 0xFF0A246A

// More authentic Windows XP colors
#define UI_DESKTOP_GRADIENT_TOP 0xFF0054E3
#define UI_DESKTOP_GRADIENT_BOTTOM 0xFF0A246A
#define UI_TASKBAR_TOP 0xFFF7F7F7
#define UI_TASKBAR_BOTTOM 0xFFECECEC
#define UI_START_BUTTON_TOP 0xFFFDFDFD
#define UI_START_BUTTON_BOTTOM 0xFFE1E1E1

static inline uint32_t lerp(uint32_t a, uint32_t b, int t, int max) {
  uint32_t ar = (a >> 16) & 0xFF;
  uint32_t ag = (a >> 8) & 0xFF;
  uint32_t ab = a & 0xFF;
  uint32_t br = (b >> 16) & 0xFF;
  uint32_t bg = (b >> 8) & 0xFF;
  uint32_t bb = b & 0xFF;
  uint32_t r = ar + ((br - ar) * t) / max;
  uint32_t g = ag + ((bg - ag) * t) / max;
  uint32_t bl = ab + ((bb - ab) * t) / max;
  return (r << 16) | (g << 8) | bl;
}

int app_count = 0;
AppDefinition apps[32];
char* active_app = NULL;

extern int open_app_count;
extern char* open_apps[];
extern void launch_app(AppDefinition* app);
extern uint64_t timer_ms(void);

int is_app_open(const char* id) {
  for (int i = 0; i < open_app_count; i++)
    if (strncmp(open_apps[i], id, strlen(id)) == 0)
      return 1;
  return 0;
}

typedef struct {
  int menu_open;
  uint64_t last_clock;
  char time_str[16];
  char date_str[16];
} TaskbarState;

TaskbarState taskbar = {0, 0, "", ""};

void update_clock() {
  uint64_t now = timer_ms();
  if (now - taskbar.last_clock < 1000)
    return;
  taskbar.last_clock = now;

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);

  strftime(taskbar.time_str, sizeof(taskbar.time_str), "%H:%M", tm);
  strftime(taskbar.date_str, sizeof(taskbar.date_str), "%m/%d/%Y", tm);
}

void taskbar_click(BootInfo* info, int mx, int my) {
  int y = info->height - 32;

  if (my < y) return;

  if (mx >= 6 && mx <= 76) {
    taskbar.menu_open = !taskbar.menu_open;
    return;
  }

  int cx = info->width / 2;
  int x = cx - (app_count * 40) / 2;

  for (int i = 0; i < app_count; i++) {
    if (mx >= x && mx <= x + 40) {
      launch_app(&apps[i]);
      return;
    }
    x += 40;
  }
}

void draw_shadow(BootInfo *info, int x, int y, int w, int h) {
  for (int i = 0; i < 8; i++) {
    uint32_t a = (0x30 - i * 6) << 24;
    fill_rect(info, x + i, y + h + i, w, 1, a);
    fill_rect(info, x + w + i, y + i, 1, h, a);
  }
}

void draw_titlebar(BootInfo *info, int x, int y, int w, int active) {
  uint32_t top = active ? UI_TITLE_ACTIVE_TOP : UI_TITLE_GRAD_TOP;
  uint32_t bottom = active ? UI_TITLE_ACTIVE_BOTTOM : UI_TITLE_GRAD_BOTTOM;
  for (int i = 0; i < 24; i++) {
    uint32_t c = lerp(top, bottom, i, 24);
    fill_rect(info, x, y + i, w, 1, c);
  }
  for (int i = 0; i < 12; i++) {
    uint32_t c = lerp(0x40FFFFFF, 0x00000000, i, 12);
    fill_rect(info, x, y + i, w, 1, c);
  }
}

void draw_glass_button(BootInfo *info, int x, int y, int w, int h,
                       const char *text) {
  for (int i = 0; i < h; i++) {
    uint32_t c = lerp(UI_BUTTON_TOP, UI_BUTTON_BOTTOM, i, h);
    fill_rect(info, x, y + i, w, 1, c);
  }
  for (int i = 0; i < h / 2; i++) {
    uint32_t c = lerp(0x40FFFFFF, 0x00000000, i, h / 2);
    fill_rect(info, x, y + i, w, 1, c);
  }
  draw_rect(info, x, y, w, h, UI_BORDER_MEDIUM);
  if (text) {
    int tx = x + (w - strlen(text) * 8) / 2;
    int ty = y + (h - 8) / 2;
    kprint(info, text, tx, ty, 0xFF000000);
  }
}

void draw_icon_glow(BootInfo *info, int x, int y) {
  for (int i = 0; i < 12; i++) {
    uint32_t a = (0x20 - i * 2) << 24;
    fill_rect(info, x - i, y - i, 32 + i * 2, 32 + i * 2, a);
  }
}

void draw_winxp_icon(BootInfo *info, int x, int y, const char *label) {
  draw_icon_glow(info, x, y);
  fill_rect(info, x, y, 32, 32, 0xFFFFFFFF);
  draw_rect(info, x, y, 32, 32, UI_BORDER_MEDIUM);
  uint16_t icon[16];

  // Choose icon based on label
  if (label && strcmp(label, "My Computer") == 0) {
    // Computer icon
    uint16_t computer_icon[] = {
      0x0000, 0x0000, 0x1FF8, 0x2004, 0x4002, 0x4002, 0x4002, 0x4002,
      0x4002, 0x4002, 0x4002, 0x4002, 0x3FFC, 0x0000, 0x0000, 0x0000
    };
    memcpy(icon, computer_icon, sizeof(icon));
  } else if (label && strcmp(label, "Recycle Bin") == 0) {
    // Recycle bin icon
    uint16_t recycle_icon[] = {
      0x0000, 0x0000, 0x0FF0, 0x1008, 0x2004, 0x4002, 0x87C1, 0x8811,
      0x8811, 0x87C1, 0x4002, 0x2004, 0x1008, 0x0FF0, 0x0000, 0x0000
    };
    memcpy(icon, recycle_icon, sizeof(icon));
  } else if (label && strcmp(label, "Doom") == 0) {
    // Doom icon (skull)
    uint16_t doom_icon[] = {
      0x0000, 0x0000, 0x0E70, 0x1118, 0x2084, 0x4042, 0x8041, 0x8041,
      0x8041, 0x8041, 0x4042, 0x2084, 0x1118, 0x0E70, 0x0000, 0x0000
    };
    memcpy(icon, doom_icon, sizeof(icon));
  } else {
    // Default folder icon
    uint16_t folder_icon[] = {
      0x0000, 0x0000, 0x0E00, 0x1100, 0x1080, 0x7FFC, 0x4002, 0x4002,
      0x4002, 0x4002, 0x4002, 0x4002, 0x7FFE, 0x0000, 0x0000, 0x0000
    };
    memcpy(icon, folder_icon, sizeof(icon));
  }

  for (int r = 0; r < 16; r++) {
    uint16_t m = icon[r];
    for (int c = 0; c < 16; c++) {
      if ((m >> (15 - c)) & 1) {
        uint32_t color = 0xFF000000; // Black by default
        if (label && strcmp(label, "My Computer") == 0) color = 0xFF000080; // Purple
        else if (label && strcmp(label, "Recycle Bin") == 0) color = 0xFF008000; // Green
        else if (label && strcmp(label, "Doom") == 0) color = 0xFF800000; // Maroon
        fill_rect(info, x + c * 2, y + r * 2, 2, 2, color);
      }
    }
  }
  if (label) {
    int lx = x + (32 - strlen(label) * 8) / 2;
    kprint(info, label, lx, y + 36, 0xFFFFFFFF);
  }
}

void draw_winxp_window(BootInfo *info, int x, int y, int w, int h,
                       const char *title, int active) {
  draw_shadow(info, x, y, w, h);
  fill_rect(info, x, y, w, h, 0xFFFFFFFF);
  draw_titlebar(info, x, y, w, active);
  draw_rect(info, x, y, w, h, UI_BORDER_DARK);
  if (title)
    kprint(info, title, x + 10, y + 6, 0xFFFFFFFF);
  draw_glass_button(info, x + w - 50, y + 4, 18, 16, "X");
  draw_glass_button(info, x + w - 72, y + 4, 18, 16, "");
  draw_glass_button(info, x + w - 94, y + 4, 18, 16, "");
}

void draw_start_menu(BootInfo *info) {
  if (!taskbar.menu_open)
    return;

  int w = 220;
  int h = 260;
  int x = 6;
  int y = info->height - 32 - h - 4;

  fill_rect(info, x, y, w, h, 0xF0151520);
  draw_rect(info, x, y, w, h, UI_BORDER_DARK);

  kprint(info, "Applications", x + 10, y + 10, 0xFFE0E0E0);

  int ay = y + 40;
  for (int i = 0; i < app_count; i++) {
    kprint(info, apps[i].name, x + 20, ay, 0xFFFFFFFF);
    ay += 20;
  }

  kprint(info, "Shut Down", x + 20, y + h - 30, 0xFFFF8080);
}

void draw_dock(BootInfo *info) {
  int y = info->height - 32;
  int cx = info->width / 2;
  int x = cx - (app_count * 40) / 2;

  for (int i = 0; i < app_count; i++) {
    int iy = y + 4;
    if (active_app && strncmp(active_app, apps[i].id, strlen(apps[i].id)) == 0)
      iy -= 3;

    fill_rect(info, x, iy, 32, 24, 0x30FFFFFF);
    draw_rect(info, x, iy, 32, 24, UI_BORDER_MEDIUM);

    if (is_app_open(apps[i].id))
      fill_rect(info, x + 14, y + 28, 4, 4, 0xFF60A0FF);

    x += 40;
  }
}

void draw_clock(BootInfo *info) {
  int y = info->height - 32;
  int x = info->width - 90;

  fill_rect(info, x, y + 4, 84, 24, 0xFFFFFFFF);
  draw_rect(info, x, y + 4, 84, 24, UI_BORDER_MEDIUM);

  kprint(info, taskbar.time_str, x + 8, y + 8, 0xFF000000);
}

void draw_winxp_taskbar(BootInfo *info) {
  update_clock();

  int h = 32;
  int y = info->height - h;

  // Taskbar background is already drawn in draw_winxp_desktop

  draw_rect(info, 0, y, info->width, h, UI_BORDER_DARK);

  draw_glass_button(info, 6, y + 4, 70, 24, "Start");

  draw_dock(info);
  draw_clock(info);
  draw_start_menu(info);
}

void draw_winxp_terminal(BootInfo *info, int x, int y, int w, int h) {
  draw_winxp_window(info, x, y, w, h, "Command Prompt", 1);
  fill_rect(info, x + 2, y + 26, w - 4, h - 28, 0xFF000000);
  draw_rect(info, x + 1, y + 25, w - 2, h - 26, UI_BORDER_MEDIUM);
}

void draw_winxp_desktop(BootInfo *info) {
  // Draw gradient desktop background
  for (int y = 0; y < info->height - 40; y++) { // Leave space for taskbar
    uint32_t c = lerp(UI_DESKTOP_GRADIENT_TOP, UI_DESKTOP_GRADIENT_BOTTOM, y, info->height - 40);
    fill_rect(info, 0, y, info->width, 1, c);
  }

  // Draw taskbar area background
  for (int y = info->height - 40; y < info->height; y++) {
    uint32_t c = lerp(UI_TASKBAR_TOP, UI_TASKBAR_BOTTOM, y - (info->height - 40), 40);
    fill_rect(info, 0, y, info->width, 1, c);
  }
}

void init_winxp_desktop(BootInfo *info) {
  draw_winxp_desktop(info);
  draw_winxp_taskbar(info);

  // Desktop icons
  draw_winxp_icon(info, 50, 50, "My Computer");
  draw_winxp_icon(info, 50, 120, "Recycle Bin");
  draw_winxp_icon(info, 50, 190, "My Documents");
  draw_winxp_icon(info, 50, 260, "Doom");

  // Terminal window
  draw_winxp_terminal(info, 200, 100, 600, 400);

  // Welcome message
  kprint(info, "Welcome to Tiny64 OS!", 300, 50, 0xFFFFFFFF);
  kprint(info, "Type 'help' for commands", 300, 70, 0xFFCCCCCC);
}

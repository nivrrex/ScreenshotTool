# ScreenshotTool
Windows下简单的快捷键截图工具，Alt+A进行截图，截图结果保存在剪贴板里，有系统托盘功能。

# 编译
x86_64-w64-mingw32-g++ -O2 -mwindows ScreenshotTool.cpp -lgdi32 -luser32 -lshell32 -static -o ScreenshotTool.exe
x86_64-w64-mingw32-strip --strip-unneeded ScreenshotTool.exe

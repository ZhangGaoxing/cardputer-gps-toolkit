/**
 * sub_function.h — 子功能抽象基类
 * 所有菜单中的子功能都必须继承此类并实现生命周期方法
 *
 * SubFunction 同时继承 IKeyListener，因此可以直接传给 KeyboardManager::setListener()
 */
#ifndef SUB_FUNCTION_H
#define SUB_FUNCTION_H

#include <M5Cardputer.h>
#include "../config.h"
#include "../display_manager.h"
#include "../keyboard_manager.h"

/**
 * SubFunction — 子功能抽象基类 + IKeyListener
 *
 * 生命周期：
 *   1. onEnter()  — 进入子功能，初始化状态
 *   2. onUpdate() — 每帧/定时调用，绘制内容
 *   3. onKeyEvent() — 处理按键输入（IKeyListener接口）
 *   4. onExit()  — 退出子功能，清理状态
 */
class SubFunction : public IKeyListener {
public:
  /**
   * 构造函数
   * @param name    英文名称（菜单显示用）
   * @param iconType 图标类型（IconType枚举，决定绘制哪个几何图标）
   */
  SubFunction(const char* name, int iconType)
    : _name(name), _iconType(iconType) {}
  virtual ~SubFunction() = default;

  // ======== 生命周期方法（子类必须/可选实现）========

  /** 进入子功能时调用，用于初始化状态 */
  virtual void onEnter() = 0;

  /** 退出子功能时调用，用于清理状态（默认空实现） */
  virtual void onExit() {}

  /**
   * 绘制子功能内容
   * @param force true=强制重绘（如按键触发），false=间隔刷新
   */
  virtual void onUpdate(bool force) = 0;

  /**
   * 是否需要在本轮周期刷新时重绘
   * 默认保持现有行为：总是允许重绘。
   */
  virtual bool needsRedraw(unsigned long now) { (void)now; return true; }

  /**
   * 处理按键事件（IKeyListener接口实现）
   * 默认不处理，返回false让事件继续传递
   * @param event 按键事件
   * @return true=事件已消费，false=未处理
   */
  bool onKeyEvent(const KeyEvent& event) override { return false; }

  /**
   * 当返回 true 时，ESC/` 不会被 EscInterceptor 拦截，
   * 而是传递给本函数的 onKeyEvent() 自行处理。
   * 用于有内部子页面的函数（如 Waypoint 的 List→Detail→Create 导航）。
   */
  virtual bool wantsEsc() { return false; }

  // ======== 显示属性 ========

  /** 内容更新间隔(ms)，默认1000ms（1Hz刷新） */
  virtual unsigned long updateInterval() const { return _updateInterval; }

  // ======== 菜单显示 ========

  /** 获取功能名称 */
  const char* displayName() const { return _name; }

  /** 获取图标类型 */
  int iconType() const { return _iconType; }

  /**
   * 绘制菜单图标（使用几何图形，零PROGMEM开销）
   * @param x     图标左上角X坐标
   * @param y     图标左上角Y坐标
   * @param size  图标尺寸（正方形边长）
   * @param color 绘制颜色（RGB565）
   */
  virtual void drawIcon(int x, int y, int size, uint16_t color) = 0;

protected:
  /** 获取当前画布引用（快捷方式） */
  M5Canvas& canvas() { return DisplayManager::instance().canvas(); }

  /** 设置更新间隔 */
  void setUpdateInterval(unsigned long ms) { _updateInterval = ms; }

private:
  const char* _name;
  int _iconType;
  unsigned long _updateInterval = 1000;
};

#endif // SUB_FUNCTION_H

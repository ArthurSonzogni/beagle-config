#include <fstream>
#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ui/panel/panel.hpp"
#include <sstream>
#include "utils.hpp"

#define LEDS_PATH "/sys/class/leds/"

using namespace ftxui;

namespace ui {

std::vector<std::string> FindLEDs() {
  std::vector<std::string> names;

  procxx::process ls{"ls"};
  ls.add_argument("-C1");
  ls.add_argument("/sys/class/leds/");
  ls.exec();

  std::string name;
  while (std::getline(ls.output(), name))
    names.push_back(name);
  return names;
}

// A component displaying an individual LED.
class Led : public ComponentBase {
 public:
  Led(std::string name) : name_(name), file_path_("/sys/class/leds/" + name) {
    FetchState();

    Add(Container::Vertical({
        trigger_list_,
        button_toggle_,
        Container::Vertical({
            slider_period_,
            slider_delay,
            button_trigger_timer_,
        }),
    }));
  }

 private:
  void FetchState() {
    std::ifstream(file_path_ + "/brightness") >> brightness_;
    trigger_entries_.clear();
    trigger_selected_ = 0;

    std::ifstream file(file_path_ + "/trigger");
    std::string trigger;
    std::getline(file, trigger);
    
    std::string entry;
    std::stringstream ss(trigger);
    while (std::getline(ss, entry, ' ')) {
      if (entry.front() == '[')
        trigger_selected_ = trigger_entries_.size();
      trigger_entries_.push_back(to_wstring(entry));
    }

    size_t left = trigger.find('[');
    size_t right = trigger.find(']');
    if (left < right && left != std::string::npos && right != std::string::npos)
      trigger_ = trigger.substr(left + 1, right - left - 1);
    else
      trigger_ = "error";
  }

  void Toggle() {
    brightness_ = !brightness_;
    std::ofstream(file_path_ + "/brightness") << brightness_;
    std::ofstream(file_path_ + "/trigger") << "none";
    FetchState();
  }

  void TriggerTimer() {
    std::ofstream(file_path_ + "/trigger") << "timer";
    std::ofstream(file_path_ + "/delay_on")
        << std::to_string(int(ratio_ * period_));
    std::ofstream(file_path_ + "/delay_off")
        << std::to_string(int((1.f - ratio_) * period_));
    FetchState();
  }

  Element Render() override {
    return vbox({
        hbox(text(L"Name      :") | bold, text(to_wstring(name_))),
        hbox(text(L"Brightness:") | bold, text(to_wstring(brightness_))),
        hbox(text(L"Trigger   :") | bold, text(to_wstring(trigger_))),
        separator(),
        hbox(separator(),
             trigger_list_->Render() | yframe | size(HEIGHT, EQUAL, 10)),
        separator(),
        button_toggle_->Render(),
        hbox({
            slider_period_->Render(),
            text(to_wstring(period_) + L"ms") | size(WIDTH, EQUAL, 6),
        }),
        hbox({
            slider_delay->Render(),
            text(to_wstring(int(ratio_ * 100)) + L"%") | size(WIDTH, EQUAL, 6),
        }),
        button_trigger_timer_->Render(),
    });
  }

  bool OnEvent(Event event) override {
    int trigger_selected = trigger_selected_;
    bool ret = ComponentBase::OnEvent(event);
    if (trigger_selected != trigger_selected_) {
      std::ofstream(file_path_ + "/trigger")
          << to_string(trigger_entries_[trigger_selected_]);
      FetchState();
    }
    return ret;
  }

  const std::string name_;
  const std::string file_path_;
  int brightness_ = 0;
  int period_ = 20;
  float ratio_ = 0.5f;
  std::string trigger_;

  int trigger_selected_ = 0;
  std::vector<std::wstring> trigger_entries_;

  Component trigger_list_ = Radiobox(&trigger_entries_, &trigger_selected_);
  Component button_toggle_ = Button(L"Toggle", [this] { Toggle(); });
  Component button_trigger_timer_ =
      Button(L"Trigger on timer ", [this] { TriggerTimer(); });
  Component slider_period_ = Slider(L"Period      :", &period_, 0, 2000, 50);
  Component slider_delay = Slider(L"Ratio ON/OFF:", &ratio_, 0.f, 1.f, 0.05f);
};

// A panel displaying all the LEDs.
class LedPanel : public PanelBase {
 public:
  LedPanel() {
    for (auto name : FindLEDs()) {
      names_.push_back(to_wstring(name));
      led_tab_->Add(Make<Led>(name));
    }

    Add(Container::Vertical({
        radiobox_,
        led_tab_,
    }));
  }
  ~LedPanel() override = default;

 private:
  Element Render() override {
    return vbox({
               text(L"Select a LED:"),
               hbox(text(L" "), radiobox_->Render()),
               separator(),
               led_tab_->Render(),
           }) |
           frame;
  }

  std::wstring Title() override { return L"LEDs"; }

  int selected_led_ = 0;
  std::vector<std::wstring> names_;

  Component radiobox_ = Radiobox(&names_, &selected_led_);
  Component led_tab_ = Container::Tab({}, &selected_led_);
};

namespace panel {
Panel Led() {
  return Make<LedPanel>();
}

}  // namespace panel

}  // namespace ui

#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto_gui/yocto_gui.h>

#include "Denoiser.h"
#include "MultiscaleDenoiser.h"
#include "IDenoiser.h"
#include "SpikeRemovalFilter.h"
#include "ParametersIO.h"
#include "ImageIO.h"
#include "DeepImage.h"
#include "Chronometer.h"
#include "Utils.h"
#include <iostream>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <memory>

using namespace yocto::math;
namespace img = yocto::image;
namespace cli = yocto::commonio;
namespace gui = yocto::gui;


#include <atomic>
#include <deque>
#include <future>

#include "ext/filesystem.hpp"
namespace sfs = ghc::filesystem;

struct app_state {
  // original data
  std::string name     = "";
  std::string filename = "";
  std::string outname  = "";

  // viewing properties
  gui::image*       glimage   = new gui::image{};
  gui::image_params glparams  = {};
  bool              glupdated = true;

  // loading status
  std::atomic<bool> ok           = false;
  std::future<void> loader       = {};
  std::string       status       = "";
  std::string       error        = "";
  std::string       loader_error = "";

  // cleanup
  ~app_state() {
    if (glimage) delete glimage;
  }
};

// app states
struct app_states {
  // data
  std::vector<app_state*> states   = {};
  app_state*              selected = nullptr;
  std::deque<app_state*>  loading  = {};

  // default options
  float                  exposure = 0;
  bool                   filmic   = false;
  img::colorgrade_params params   = {};

  // cleanup
  ~app_states() {
    for (auto state : states) delete state;
  }
};


int main(int argc, const char* argv[]) {
  // prepare application
  auto apps_guard = std::make_unique<app_states>();
  auto apps       = apps_guard.get();
  auto filenames  = std::vector<std::string>{};

  // command line options
  auto cli = cli::make_cli("yimgview", "view images");
  add_option(cli, "images", filenames, "image filenames", true);
  parse_cli(cli, argc, argv);

  // loading images
 

  // callbacks
  auto callbacks     = gui::ui_callbacks{};
  callbacks.clear_cb = [apps](gui::window* win, const gui::input& input) {
    for (auto app : apps->states) clear_image(app->glimage);
  };
  
  callbacks.uiupdate_cb = [apps](gui::window* win, const gui::input& input) {
    if (!apps->selected) return;
    auto app = apps->selected;
    // handle mouse
    if (input.mouse_left && !input.widgets_active) {
      app->glparams.center += input.mouse_pos - input.mouse_last;
    }
    if (input.mouse_right && !input.widgets_active) {
      app->glparams.scale *= powf(
          2, (input.mouse_pos.x - input.mouse_last.x) * 0.001f);
    }
  };
  

  // run ui
  run_ui({1280 + 320, 720}, "yimview", callbacks);

  // done
  return 0;
}
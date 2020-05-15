#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>

using namespace yocto::math;
namespace img = yocto::image;
namespace cli = yocto::commonio;

using namespace std::string_literals;

#include "ext/filesystem.hpp"
namespace sfs = ghc::filesystem;


int main(int argc, const char* argv[]) {
  // command line parameters
  auto output = "out.png"s;
  auto filename = "img.hdr"s;

  // parse command line
  auto cli = cli::make_cli("yimgdenoise", "Denoise images");
  add_option(cli, "--output,-o", output, "output image filename");
  add_option(cli, "filename", filename, "input image filename", true);
  parse_cli(cli, argc, argv);

  // error std::string buffer
  auto error = ""s;

  // load
  auto ext      = sfs::path(filename).extension().string();
  auto basename = sfs::path(filename).stem().string();
  auto ioerror  = ""s;
  auto img      = img::image<vec4f>{};
  if (!load_image(filename, img, ioerror)) cli::print_fatal(ioerror);
  

  // denoise image

  // save image
  //if (!save_image(output, logo ? add_logo(img) : img, ioerror))
  //  cli::print_fatal(ioerror);

  // done
  return 0;
}
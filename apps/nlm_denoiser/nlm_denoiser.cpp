#include <iostream>
#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
#include <yocto/yocto_shape.h>
#include <yocto/yocto_sceneio.h>
#include "ext/filesystem.hpp"

using namespace yocto::math;
using namespace std::string_literals;

namespace fs = ghc::filesystem;
namespace cli = yocto::commonio;
namespace sio = yocto::sceneio;
namespace shp = yocto::shape;
namespace img  = yocto::image;

//compute euclidean distance
float distance_patches(img::image<vec4f> &img, vec2i& pcoords, vec2i& qcoords, int r){

    auto pi = pcoords.x;
    auto pj = pcoords.y;
    auto qi = qcoords.x;
    auto qj = qcoords.y;

    auto d = 0.0f;

    for(auto rj = -r; rj <= r; rj++){
        for(auto ri = -r; ri <= r ; ri++){
            if(!img.contains({qi + ri, qj + rj})) continue;
            if(!img.contains({pi + ri, pj + rj})) continue;
            auto p = img[{pi + ri, pj + rj}] * 255;
            auto q = img[{qi + ri, qj + rj}] * 255;
            
            d += distance_squared({p.x, p.y, p.z},{q.x, q.y, q.z});
        }
    }
    d/= pow(2*r + 1.0f, 2);

    //done
    return d;
}

//gaussian weighting function
float compute_weight(float distance, float h_filter, float sigma){
    
    //apply gaussian weighting
    auto exponent = -max(distance - 2.0f * pow(sigma, 2), 0.0f) / pow(h_filter, 2);
    float weight = expf(exponent);

    //done
    return weight;
}

//implementation of non-local means denoiser
void nlmeans(img::image<vec4f>& img, img::image<vec4f>& output, int radius, 
            int big_radius, float h_filter, float sigma){
    
    //image size
    auto w = img.size().x;
    auto h = img.size().y;

    auto iteration = 0.0f;
    //iterating the image
    for(auto j = 0; j < h; j++){

        //command line goodness
        auto percentage = iteration/(w*h) * 100;
        auto string_percentage = "Progress: " +  std::to_string(percentage) + " %";
        std::cout << "\r" << string_percentage;
        iteration += w;

        for(auto i = 0; i < w; i++){

            //get the pixel
            auto& p = img[{i,j}];

            //computing the window of p of radius big_r
            //clamp to avoid index out of bound
            auto start_horizontal = clamp(i - big_radius, 0, w - 1);
            auto end_horizontal   = clamp(i + big_radius, 0, w - 1);
            auto start_vertical   = clamp(j - big_radius, 0, h - 1);
            auto end_vertical     = clamp(j + big_radius, 0, h - 1);

            //C(p)
            auto c = 0.0f;

            //color
            auto color = vec4f{0,0,0, 1};

            //iterate the search window
            for(auto k = start_vertical; k < end_vertical; k++){
                for(auto l = start_horizontal; l < end_horizontal; l++){

                    //if we are in the same pixel -> skip
                    //if(k == j && l == i) continue;
                    //get the pixel
                    auto&q = img[{l, k}];
                    auto d = distance_patches(img, vec2i{i, j}, vec2i{l, k}, radius);
                    auto weight = compute_weight(d, h_filter, sigma);
                    c += weight;
                    color += q * weight;
                }
            }

            //security check
            if(c > 0) color /= c;
            color.w = 1;

            //update output image
            output[{i,j}] = color;
        }
    }
}

int main(int argc, const char* argv[]) {

    //params
    auto input_path = ""s;
    auto output_path = ""s;
    auto patch_r = 0;
    auto big_r = 0;
    auto h= 0.0f;
    auto sigma = 0.0f;
    
    // parse command line
    auto cli = cli::make_cli("nlm_denoiser", "Non-local means denoiser");
    add_option(cli, "--input", input_path, "Input path");
    add_option(cli, "--output", output_path, "Output path");
    add_option(cli, "--patch_r", patch_r, "Patch radius");
    add_option(cli, "--big_r", big_r, "Window radius");
    add_option(cli, "--h", h, "h");
    add_option(cli, "--sigma", sigma, "sigma");
    parse_cli(cli, argc, argv);

    auto h_filter = h * sigma;

    //loading image
    auto img = img::image<vec4f>();

    // error buffer
    auto ioerror = "error loading the image"s;
    
    //loading image
    std::cout << "Loading image from " + input_path << std::endl;

    //load image
    if(!load_image(input_path, img, ioerror)){
        cli::print_fatal(ioerror);
        return 0;
    }

    //output image
    auto output = img::image<vec4f>(img.size());

    //processing
    std::cout << "Processing..." << std::endl;

    //apply the algorithm
    nlmeans(img, output, patch_r, big_r, h_filter, sigma);

    //saving image
    std::cout << "Saving " + output_path << std::endl;

    // save
    if (!save_image(output_path, output, ioerror)){
        cli::print_fatal(ioerror);
    }
        
    // done
    return 0;

}


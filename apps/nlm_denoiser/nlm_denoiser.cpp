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

double e = 2.7182818284590;
float ef = (float) e;


float compute_neighborhood_mean(img::image<vec4f> &img, vec2i& pix_coords, int radius){

    //params
    auto i = pix_coords.x;
    auto j = pix_coords.y;
    auto w = img.size().x;
    auto h = img.size().y;
    auto start_horizontal = clamp(i - radius, 0, w - 1);
    auto end_horizontal   = clamp(i + radius, 0, w - 1);
    auto start_vertical   = clamp(j - radius, 0, h - 1);
    auto end_vertical     = clamp(j + radius, 0, h - 1);

    //mean
    auto mean = 0.0f;
    auto count = 0;

    for(auto k = start_vertical; k < end_vertical; k++){
        for(auto l = start_horizontal; l < end_horizontal; l++){
            auto& p = img[{l,k}];
            mean += (p.x + p.y + p.z)*255;
            count+=3;
        }
    }

    //security check
    if(count > 0) mean/=count;

    //done
    return mean;
}

//gaussian weighting function
float compute_weight(img::image<vec4f>& img,  vec2i q_coords, int radius, float bp,
            float h_filter, float sigma){
    
    //compute euclidean distance
    float distance = pow(compute_neighborhood_mean(img, q_coords, radius) - bp, 2);
    
    //apply gaussian weighting
    auto exp = -max(distance - 2.0f * pow(sigma, 2), 0.0f) / pow(h_filter, 2);

    float weight = pow(e, exp);

    //done
    return weight;
}

//implementation of pixelwise non-local means denoiser
void pixelwise_NLM(img::image<vec4f>& img, img::image<vec4f>& output, int radius,
            float h_filter, float sigma){
    
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

            //computing the window of p of radius r
            //clamp to avoid index out of bound
            auto start_horizontal = clamp(i - radius, 0, w - 1);
            auto end_horizontal   = clamp(i + radius, 0, w - 1);
            auto start_vertical   = clamp(j - radius, 0, h - 1);
            auto end_vertical     = clamp(j + radius, 0, h - 1);

            //C(p)
            auto c = 0.0f;

            //color
            auto color = vec4f{0,0,0, 1};

            //neighborhood's mean B(p,r)
            auto bp = compute_neighborhood_mean(img, vec2i(i,j), radius);

            //iterate the neighborhood
            for(auto k = start_vertical; k < end_vertical; k++){
                for(auto l = start_horizontal; l < end_horizontal; l++){

                    //if we are in the same pixel -> skip
                    if(k == j && l == i) continue;
                    //get the pixel
                    auto& q = img[{l,k}];
                    auto weight = compute_weight(img, vec2i{l,k}, radius, bp, h_filter, sigma);
                    c += weight;
                    color += q * weight;
                }
            }

            //security check
            if(c > 0) color /= c;

            //update output image
            output[{i,j}] = color;
        }
    }
}

int main(int argc, const char* argv[]) {

    //params
    auto input_path = ""s;
    auto output_path = ""s;
    auto radius = 0;
    auto h= 0.0f;
    auto sigma = 0.0f;
    
    // parse command line
    auto cli = cli::make_cli("nlm_denoiser", "Non-local means denoiser");
    add_option(cli, "--input", input_path, "Input path");
    add_option(cli, "--output", output_path, "Output path");
    add_option(cli, "--radius", radius, "radius");
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
    pixelwise_NLM(img, output, radius, h_filter, sigma);

    //saving image
    std::cout << "Saving " + output_path << std::endl;

    // save
    if (!save_image(output_path, output, ioerror)){
        cli::print_fatal(ioerror);
    }
        
    // done
    return 0;

}


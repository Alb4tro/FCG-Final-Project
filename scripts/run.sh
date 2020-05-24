#./bin/RelWithDebInfo/yscenetrace tests/01_cornellbox/cornellbox.json -o out/lowres/01_cornellbox_512_128.jpg -t path -s 128 -r 512
#./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/01_cornellbox_512_128
#rm out/file.raw 

./bin/RelWithDebInfo/yscenetrace tests/11_bathroom1/bathroom1.json -o out/lowres/11_bathroom_720_128.jpg -t path -s 128 -r 720
./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/11_bathroom_720_128
rm out/file.raw 

#./bin/RelWithDebInfo/yscenetrace tests/13_bedroom/bedroom.json -o out/lowres/13_bedroom_720_128.jpg -t path -s 128 -r 720
#./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/13_bedroom_720_128
#rm out/file.raw 

#./bin/RelWithDebInfo/yscenetrace tests/17_kitchen/kitchen.json -o out/lowres/17_kitchen_720_128.jpg -t path -s 128 -r 720
#./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/13_kitchen_720_128
#rm out/file.raw 
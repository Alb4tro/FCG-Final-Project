#./bin/RelWithDebInfo/yscenetrace tests/01_cornellbox/cornellbox.json -o out/01_cornellbox_512_64.jpg -t path -s 64 -r 512
./bin/RelWithDebInfo/yscenetrace tests/12_ecosys/ecosys.json -o out/lowres/12_ecosys_720_64.jpg -t path -s 64 -r 720
./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/12_ecosys_720_64
rm out/file.raw 


#./bin/yimagedenoise tests/test.raw -o out/not_noise_image -d 2 -b 8 -w 2 -r 0 -p 0 --p-factor 1.5 -m 0.5 -s 4 --ncores 2 


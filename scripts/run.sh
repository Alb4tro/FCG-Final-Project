./bin/RelWithDebInfo/yscenetrace tests/01_cornellbox/cornellbox.json -o out/lowres/01_cornellbox_512_64.jpg -t path -s 64 -r 512
./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/bcd_denoised_images/01_cornellbox_512_64
rm out/file.raw 

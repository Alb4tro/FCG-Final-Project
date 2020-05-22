./bin/RelWithDebInfo/yscenetrace tests/01_cornellbox/cornellbox.json -o out/01_cornellbox_512_64.jpg -t path -s 64 -r 512
./bin/RelWithDebInfo/yimagedenoise out/file.raw -o out/not_noise_image 
rm out/file.raw 
#./bin/yimagedenoise tests/test.raw -o out/not_noise_image -d 2 -b 8 --ncores 2
#./bin/yimagedenoise tests/test.raw -o out/not_noise_image -d 2 -b 8 -w 2 -r 0 -p 0 --p-factor 1.5 -m 0.5 -s 4 --ncores 2 


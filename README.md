# FCG-Final-Project

Versione temporanea in italiano

## Lavoro svolto
1. Abbiamo integrato il bayesian collaborative denoising (BCD) all'interno di yocto. Per fare ciò abbiamo dovuto:
    * Inserire la libreria di BCD all'interno della directory libs
    * Modificare diversi CMakeList (anche quelli all'interno di BCD che presentavano un path assoluto)
    * Scrivere una nuova app, localizzata dentro apps, chiamata yimagedenoise che si occupa di eseguire il denoising dell'immagine passata in input utilizzando BCD
2. Abbiamo implementato un NLM denoiser nell'app nlm_denoiser.

Di seguito riportiamo i risultati ottenuti:

Noisy image:
![Image](out\lowres\01_cornellbox_512_256.jpg)
Denoised image by BCD:
![Image](out\bcd_denoised_images\01_cornellbox_512_256_denoised.png)
Denoised image by our NLM:
![Image](out\nlm_denoised_images\denoised_cornellbox_256_8.png)
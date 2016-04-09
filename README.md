# sekio_pifm

sekiolabs modified pifm sstvstuff

gensstv is heavilly based on work from KI4MCW, which can be found here: https://sites.google.com/site/ki4mcw/Home/sstv-via-uc
I fixed some errors and made it a little bit more flexible.

pifm is based on the work of Oliver Mattos and Oskar Weigl  (http://www.icrobotics.co.uk/wiki/index.php/Turning_the_Raspberry_Pi_Into_an_FM_Transmitter).

The original program was intended for transmitting broadband stereo signals.
PA3BYA adapted it a little bit so that the bandwidth can be set, which is very important for narrow-band ham radio transmissions. 
Also the timing can be tuned from the command-line, which is important for SSTV, where impropper timing results in slanted images

sekio labs added getopt() improvements and scottie1 support

sstv_cherryblossom.png - curie and cherry blossoms
sstv_suncorporation.png - sun corporation paving the future
sstv_testbars.png - test bars 

## License
All of the code contained here is licensed by the GNU General Public License v3.

## Credits
Gerrit Polder, PA3BYA.
Credits to KI4MCW (sstv), Oliver Mattos and Oskar Weigl (pifm).

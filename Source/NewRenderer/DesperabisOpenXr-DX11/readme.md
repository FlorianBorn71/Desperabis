# Desperabis OpenXr DX11

This is an OpenXr version of the renderer, based on the "Hello Xr" sample reference from the Khronos Group.
It renders on Windows Desktop using the DX11 based renderer. But it can be streamed to Oculus Quest using the
[Oculus link feature](https://www.meta.com/help/quest/articles/headsets-and-accessories/oculus-link/connect-with-air-link).

## Run on Meta Quest

This App does not compile for or run on Quest directly. Instead, it runs on the PC but streams live to Quest using the Live link 
feature, which is a built-in feature on the Quest OS.

### Use the Quest link OpenXr runtime (one time setup)

Before starting this app on the PC, make sure the Oculus runtime is installed and active. 
For that, open the ""Meta Quest Link"" app on PC, go to the Settings -> General tab and click the 
"Set Meta Quest Link as default OpenXR runtime" (or similar) button.

### Run the app

Just run the Desperabis-OpenXr-DX11 runtime. Look out for errors in the DOS window in case it aborts immediately after starting.

### Connect from Quest

On the Quest, Open the Live link and connect to your PC. You should now see a Desperabis level in real size
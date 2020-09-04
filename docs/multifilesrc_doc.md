### Video File with gstreamer ingestor (multifilesrc)

**NOTE**:

* In case one does not want to loop the video with multifilesrc element then set the `loop` property to `FALSE`

* In case one wants to play the video for specific number of iterations set the `loop` property to `FALSE` and `stop-index` property to the number of iteration starting with 0 which would play it once. Setting the `loop` property to `TRUE` will override the `stop-index` property.

* The `loop` property of `multifilesrc` plugin does not support video files of MP4 format. Hence MP4 video files will not loop and the recommendation is to transcode the video file to AVI format.

* In case one notices general stream error with multifilesrc element when certain video files are used then transcode the video file to `H264` video with `.avi` container format to ensure the compatibity of the format of the video file.
mkdir -p downloads && cd downloads
echo "downloads directory created and switched"
curl https://www.emva.org/wp-content/uploads/GenICam_V3_1_0_public_data.zip -L -O
echo "GenICam v3.1 runtime downloaded"
find . -type f -not -name 'GenICam_V3_1_0_public_data.zip' -delete
unzip *
echo "GenICam v3.1 runtime unzipped"
find . -type f -not -name '*Runtime*Linux64*x64*3_1*.tgz' -delete
echo "GenICam v3.1 runtime for Linux64/x64 is found"
tar xzvf *
echo "GenICam runtime for Linux64/x64 untarred"
cp bin/Linux64_x64/* /usr/lib/x86_64-linux-gnu/
echo "GenICam runtime copied to /usr/lib/x86_64-linux-gnu/"
cd ..
rm -rf downloads
echo "downloads directory cleaned up"
./autogen.sh
echo "configure run"
make
echo "make successful"
make install
echo "install successful"
ldconfig
export GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0
echo "GST_PLUGIN_PATH set to /usr/local/lib/gstreamer-1.0"
echo "Please set GENICAM_GENTL64_PATH to GenTL producer directory"
echo "exiting setup"

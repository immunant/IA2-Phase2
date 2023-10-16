# Nginx RTMP module demo

We have modified the included checkout of nginx and its RTMP external module to
be compartmentalized, with the main nginx server in one compartment and the RTMP
module in another. These modifications include sharing necessary global data,
handling function pointers and callbacks that cannot be automatically annotated,
and modifying heap allocations shared between the main and module compartments.
We have added support to the nginx pool allocator for compartment-private pools
and shared pools, configurable when creating the pool.

To build:

    cd external/nginx
    ./reconfigure
    cd build/nginx
    make

The reconfigure script should handle most situations, but is really just a
convenience for demonstration purposes. See the script for more details on how
we are building nginx.

## Usage

To run the demo, you must first set up temp directories (these paths are
configured in external/nginx/conf/nginx.conf, which is then copied to the build
directory):

    mkdir -p /tmp/nginx/stream

You can then run the nginx binary (external/nginx/build/nginx/nginx). After nginx is running, upload a stream with something like the following (replace `INPUT_FILE.mp4`):

    ffmpeg -re -stream_loop -1 -i INPUT_FILE.mp4 -vcodec libx264 -preset:v ultrafast -acodec aac -f flv rtmp://localhost/stream/demo

And view the stream in a video player (VLC works) at
`rtmp://localhost/stream/demo`. Nginx will also transcode the stream to HLS in a
temp folder, just to demonstrate additional processing on the input stream.

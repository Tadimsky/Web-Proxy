# Web-Proxy
## Jonathan Schmidt

HTTP and HTTPS Web Proxy in C

Supports `GET`, `HEAD`, and `CONNECT` methods.
Added `HEAD` method to allow for getting only headers for debugging purposes.
Could easily be extended for other methods, although not done.

To compile, run `make` in the `src` folder.
To run, execute `Web-Proxy <port>` where `<port>` is the port to listen on.

##Issues encountered
One of the weirdest issues I encountered was that it worked for almost all websites, except a couple that just gave no response.
I looked into it and it appeared to be websites hosted with cloudflare, and some other CDNs. After hours of debugging, the only difference that I was able to see between a normal request and one with my proxy, was that my was sending FIN as part of the GET request.
It turns out that only these few websites would not reply to packets that had `FIN` (are they demanding persistent connections?).
To fix this, I made the `shutdown(serverfd, 1)` call happen after the server had responded to the client, not after reading EOF from the client.

To test this: try http://www.feedly.com and see what happens on other proxies.



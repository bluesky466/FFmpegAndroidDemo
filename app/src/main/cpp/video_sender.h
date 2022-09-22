#include <string>

class VideoSender {
public:
    static bool Send(const std::string& srcUrl, const std::string& destUrl);
};
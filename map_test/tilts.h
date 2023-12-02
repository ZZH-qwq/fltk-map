#pragma once
//
//  tilts.h
// 
//  Autonavi WMTS tile map service
//  Thanks @danzou1ge6 / Yu Liukun for this code snippets.
//

namespace tilts {
    const int TILT_SIZE = 256;

    class TiltId {
    public:
        int x, y, z;

        const char* UDT = "20231025";

        std::string to_request_url() {
            std::stringstream ss;
            ss << "/appmaptile?lang=zh_cn&size=1&scale=1&style=" << (z < 13 ? "8" : "7") << "&x=" << x << "&y=" << y
                << "&z=" << z;
            return ss.str();
        }

        bool operator==(const TiltId& rhs) const {
            return z == rhs.z && x == rhs.x && y == rhs.y;
        }
        bool operator<(const TiltId& rhs) const {
            return (z < rhs.z) || (z == rhs.z && x < rhs.x) ||
                (z == rhs.z && x == rhs.x && y < rhs.y);
        }

        TiltId offset(int i, int j) { return TiltId{ .x = x + i, .y = y + j, .z = z }; }

        friend std::ostream& operator<<(std::ostream& os, TiltId id) {
            os << "(" << id.x << ", " << id.y << ", " << id.z << ")";
            return os;
        }
    };

    struct TiltData {
        const unsigned char* buf;
        size_t size;

    public:
        TiltData() : size(0), buf(nullptr) {}
        TiltData(const unsigned char* buf, size_t size) : size(size), buf(buf) {}
    };

    struct TiltFuture {
        std::atomic_bool available;
        TiltId id;
        int status;
        std::string data;

        TiltFuture(TiltId id) : available(false), status(-1), id(id) {}

        static void makeRequest(httplib::Client* cli, std::string url, TiltFuture* future) {
            if (auto res = cli->Get(url)) {
                if (res->status == 200) {
                    future->data = std::move(res->body);
                }
                future->status = res->status;
            }
            future->available.store(true);
        }
    };

    class TiltsSource {
        std::map<TiltId, std::string> tilts;
        std::list<TiltId> cached;
        std::list<TiltFuture*> futures;
        std::set<TiltId> downloading;
        int size;
        httplib::Client cli;

    public:
        TiltsSource(int size) : cli("http://webrd03.is.autonavi.com"), size(size) {}

        bool cacheHas(TiltId id) { return tilts.find(id) != tilts.end(); }
        bool isDownloading(TiltId id) {
            return downloading.find(id) != downloading.end();
        }

        void download(TiltId id) {
            if (cacheHas(id) || isDownloading(id)) {
                return;
            }

            auto future = new TiltFuture(id);
            std::thread th(TiltFuture::makeRequest, &cli, id.to_request_url(), future);
            th.detach();
            futures.push_back(future);
            downloading.insert(id);
        }

        std::tuple<bool, bool> pollFutures() {
            if (futures.empty()) {
                return { false,false };
            }
            auto front = futures.front();
            futures.pop_front();

            auto oldFuturesCnt = futures.size();
            auto updated = false;
            if (front->available.load()) {

                if (front->status == 200) {
                    while (cached.size() >= size) {
                        tilts.erase(cached.front());
                        cached.pop_front();
                    }

                    cached.push_back(front->id);
                    tilts[front->id] = front->data;
                    downloading.erase(front->id);
                    updated = true;
#if DEBUG
                    std::cout << "Downloaded tilt " << front->id << std::endl;
#endif // DEBUG

                    // invalid status code
                } else {
#if DEBUG
                    std::cout << "Download tilt " << front->id
                        << " failed with status code" << front->status << std::endl;
#endif // DEBUG
                }

                delete front;

                // unavailable
            } else {
                futures.push_back(front);
            }

            return { oldFuturesCnt < futures.size(),updated };
        }

        TiltData get(TiltId id) {
            download(id);
            pollFutures();

            if (cacheHas(id)) {
                return TiltData((const unsigned char*)tilts[id].c_str(),
                    tilts[id].size());
            } else {
                return TiltData();
            }
        }
    };
} // namespace tilts
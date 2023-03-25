#define SPXE_APPLICATION
#include "../spxe.h"
#include <ctime>
#include <iostream>
#include <functional>
#include <cmath>
#include <thread>
#include <complex>
#include <complex.h>
#include <vector>
#define SIZE 512
#define WIDTH SIZE
#define HEIGHT SIZE

#define ABS(n) ((n) > 0 ? (n) : -(n))


class PXE{
public:
    void drawPixel(Px* buf, int x, int y, Px color)
    {
        if(x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
        color.a = 255;
        buf[y * WIDTH + x] = color;
    }

    void drawLine(Px* buf, int x0, int y0, int x1, int y1, Px color)
    {
        const int dx = ABS(x1 - x0);
        const int dy = -ABS(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;

        int error = dx + dy;

        while (1) {
            drawPixel(buf, x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }

            int e2 = error * 2;
            if (e2 >= dy) {
                if (x0 == x1) {
                    break;
                }

                error = error + dy;
                x0 = x0 + sx;
            }

            if (e2 <= dx) {
                if (y0 == y1) {
                    break;
                }

                error = error + dx;
                y0 = y0 + sy;
            }
        }
    }

    void drawRect(Px* buf, int x0, int y0, int x1, int y1, Px color)
    {
        drawLine(buf, x0, y0, x1, y0, color);
        drawLine(buf, x0, y1, x1, y1, color);
        drawLine(buf, x1, y0, x1, y1, color);
        drawLine(buf, x0, y0, x0, y1, color);
    }
};

struct Rectangle{
    int x, y, width, height;
};

class RenderWorker{
public:
    static std::function<Px(int, int)> Program;
    static Px* outlineBuffer;
    static Px* drawBuffer;
    PXE* pxe;
    Rectangle _workArea;

private:
    int _currentPixel;
    int _numPixelsCovered;
    bool _finished;

public:

    void Init(PXE* pxe, int x, int y, int width, int height){
        this->pxe = pxe;
        _workArea = Rectangle{x,y,width,height};
        _numPixelsCovered = width * height;
        _currentPixel = 0;
        _finished = false;
    }

    void Reset(int x, int y){
        _workArea.x = x;
        _workArea.y = y;
        _currentPixel = 0;
        _finished = false;
    }

    void DrawOutline(){
        float progress = float(_currentPixel) / _numPixelsCovered;
        pxe->drawRect(RenderWorker::outlineBuffer, _workArea.x, _workArea.y, _workArea.x + _workArea.width - 1, _workArea.y + _workArea.height - 1, Px{(unsigned char)(progress * 255.0f),(unsigned char)(progress * 255.0f),static_cast<unsigned char>(255 - (progress * 255)), 255});
    }

    bool IsFinished(){
        return _finished;
    }

    void Work(){
        //int sleepTime = rand()%4 + 2;
        while(!_finished){
            //std::this_thread::sleep_for(std::chrono::microseconds (500));
            if(_currentPixel < _numPixelsCovered){
                int localX = _currentPixel % _workArea.width;
                int localY = _currentPixel / _workArea.width;
                Px col = RenderWorker::Program(_workArea.x + localX, _workArea.y + localY);
                pxe->drawPixel(RenderWorker::drawBuffer, _workArea.x + localX, _workArea.y + localY, col);
//                std::cout << "Drawing pixel" << _currentPixel << std::endl;
                _currentPixel++;
            }else{
                _finished = true;
            }
        }


    }
};

std::function<Px(int,int)> RenderWorker::Program;
Px* RenderWorker::outlineBuffer;
Px* RenderWorker::drawBuffer;


struct Tile{
    int x, y;
};

class Dispatcher{
private:
    unsigned int _numWorkers;
    std::thread* _threads;
    RenderWorker* _workers;
    std::vector<Tile> _tileQueue;

    int _renderWorkerSize;
    int _renderWorkerXTiles, _renderWorkerYTiles;

public:
    Dispatcher(PXE* pxe, int renderWorkerSize)
    {
        _numWorkers = std::thread::hardware_concurrency();
        _threads = new std::thread[_numWorkers];
        _workers = new RenderWorker[_numWorkers];
        _tileQueue = std::vector<Tile>();
        //fill tileQueue
        _renderWorkerSize = renderWorkerSize;
        _renderWorkerXTiles = WIDTH / renderWorkerSize;
        _renderWorkerYTiles = HEIGHT / renderWorkerSize;
        int totalTiles = _renderWorkerXTiles * _renderWorkerYTiles;
        for (int i = 0; i < _renderWorkerXTiles; ++i) {
            for (int j = 0; j < _renderWorkerYTiles; ++j) {
                _tileQueue.push_back(Tile{j, i});
            }
        }

        for (int i = 0; i < _numWorkers; ++i) {
            int idx = rand() % _tileQueue.size();
            Tile t = _tileQueue.at(idx);
            _tileQueue.erase(_tileQueue.begin() + idx);
            //std::cout << "Dispatching at: " << t.x << ", " << t.y << std::endl;
            _workers[i].Init(pxe, t.x * _renderWorkerSize, t.y * _renderWorkerSize, _renderWorkerSize, _renderWorkerSize);
            _threads[i] = std::thread(&RenderWorker::Work, &_workers[i]);
        }

        for (int i = 0; i < _numWorkers; ++i) {
            _threads[i].detach();
        }
    }
    //poll each thread's worker, if it's done reschedule a new worker
    void Update()
    {
        for (int i = 0; i < _numWorkers; ++i) {
            if(_workers[i].IsFinished()) {
                if(!_tileQueue.empty()){
                    int idx = rand() % _tileQueue.size();
                    Tile t = _tileQueue.at(idx);
                    _tileQueue.erase(_tileQueue.begin() + idx);
                    std::cout << "Dispatching at: " << t.x << ", " << t.y << std::endl;
                    _workers[i].Reset( t.x * _renderWorkerSize, t.y * _renderWorkerSize);
                    _threads[i] = std::thread(&RenderWorker::Work, &_workers[i]);
                    _threads[i].detach();
                }
            }else{
                _workers[i].DrawOutline();
            }
        }
    }

    ~Dispatcher()
    {
        delete[] _workers;
        delete[] _threads;
    }
};

void paintBuffer(Px* src, Px* target){
    memcpy(target, src, sizeof(Px) * WIDTH * HEIGHT);
}

void SetProgram();

class vec3 {
public:
    vec3() : e{0,0,0} {}
    vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

    double x() const { return e[0]; }
    double y() const { return e[1]; }
    double z() const { return e[2]; }

    vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
    double operator[](int i) const { return e[i]; }
    double& operator[](int i) { return e[i]; }

    vec3& operator+=(const vec3 &v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    vec3& operator*=(const double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    vec3& operator/=(const double t) {
        return *this *= 1/t;
    }

    double length() const {
        return std::sqrt(length_squared());
    }

    double length_squared() const {
        return e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
    }

public:
    double e[3];
};

// Type aliases for vec3
using point3 = vec3;   // 3D point
using color = vec3;    // RGB color

 std::ostream& operator<<(std::ostream &out, const vec3 &v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

 vec3 operator+(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

 vec3 operator-(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

 vec3 operator*(const vec3 &u, const vec3 &v) {
    return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

 vec3 operator*(double t, const vec3 &v) {
    return vec3(t*v.e[0], t*v.e[1], t*v.e[2]);
}

 vec3 operator*(const vec3 &v, double t) {
    return t * v;
}

 vec3 operator/(vec3 v, double t) {
    return (1/t) * v;
}

 double dot(const vec3 &u, const vec3 &v) {
    return u.e[0] * v.e[0]
           + u.e[1] * v.e[1]
           + u.e[2] * v.e[2];
}

 vec3 cross(const vec3 &u, const vec3 &v) {
    return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

 vec3 unit_vector(vec3 v) {
    return v / v.length();
}

int main(void)
{
    SetProgram();
    srand(time(nullptr));

    Px* pixbuf = spxeStart("RayTracer", 1024, 1024, WIDTH, HEIGHT);
    Px* imgBuf = new Px[WIDTH * HEIGHT]{};

    RenderWorker::outlineBuffer = pixbuf;
    RenderWorker::drawBuffer = imgBuf;

    PXE pxe;

    Dispatcher d{&pxe, 32};



    bool renderComplete = false;

    bool usethread = true;

    //if(usethread)
    //std::thread dispatchThread = std::thread(&Dispatcher::SendIt, &d);

    while (spxeRun(pixbuf)) {
        if (spxeKeyPressed(ESCAPE)) {
            break;
        }


        paintBuffer(imgBuf, pixbuf);

        d.Update();
       
    }

    delete[] imgBuf;
    return spxeEnd(pixbuf);
}

class ray {
public:
    ray() {}
    ray(const point3& origin, const vec3& direction)
            : orig(origin), dir(direction)
    {}

    point3 origin() const  { return orig; }
    vec3 direction() const { return dir; }

    point3 at(double t) const {
        return orig + t*dir;
    }

public:
    point3 orig;
    vec3 dir;
};



double hit_sphere(const point3& center, double radius, const ray& r) {
    vec3 oc = r.origin() - center;
    auto a = r.direction().length_squared();
    auto half_b = dot(oc, r.direction());
    auto c = oc.length_squared() - radius*radius;
    auto discriminant = half_b*half_b - a*c;

    if (discriminant < 0) {
        return -1.0;
    } else {
        return (-half_b - sqrt(discriminant) ) / a;
    }
}


color ray_color(const ray& r) {
    auto t = hit_sphere(point3(0,0,-1), 0.5, r);
    if (t > 0.0) {
        vec3 N = unit_vector(r.at(t) - vec3(0,0,-1));
        return 0.5*color(N.x()+1, N.y()+1, N.z()+1);
    }
    vec3 unit_direction = unit_vector(r.direction());
    t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*color(1.0, 1.0, 1.0) + t*color(0.5, 0.7, 1.0);
}

void SetProgram(){
    const auto aspect_ratio = 1;

    auto viewport_height = 2.0;
    auto viewport_width = aspect_ratio * viewport_height;
    auto focal_length = 1.0;

    auto origin = point3(0, 0, 0);
    auto horizontal = vec3(viewport_width, 0, 0);
    auto vertical = vec3(0, viewport_height, 0);
    auto lower_left_corner = origin - horizontal/2 - vertical/2 - vec3(0, 0, focal_length);

    RenderWorker::Program = [origin, lower_left_corner, horizontal, vertical](int x, int y) -> Px{
        auto u = double(x) / (WIDTH-1);
        auto v = double(y) / (HEIGHT-1);
        ray r(origin, lower_left_corner + u*horizontal + v*vertical - origin);
        color pixel_color = ray_color(r);
        Px col = Px{static_cast<unsigned char>(pixel_color.x() * 255), static_cast<unsigned char>(pixel_color.y() * 255), static_cast<unsigned char>(pixel_color.z() * 255)};
        return col;
    };
}



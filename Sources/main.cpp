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
        pxe->drawRect(RenderWorker::outlineBuffer, _workArea.x, _workArea.y, _workArea.x + _workArea.width - 1, _workArea.y + _workArea.height - 1, Px{0,(unsigned char)(progress * 255.0f),(unsigned char)(progress * 255.0f), 255});
    }

    bool IsFinished(){
        return _finished;
    }

    void Work(){
        //int sleepTime = rand()%4 + 2;
        while(!_finished){
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

int main(void)
{
    SetProgram();
    srand(time(nullptr));

    Px* pixbuf = spxeStart("RayTracer", 1024, 1024, WIDTH, HEIGHT);
    Px* imgBuf = new Px[WIDTH * HEIGHT]{};

    RenderWorker::outlineBuffer = pixbuf;
    RenderWorker::drawBuffer = imgBuf;

    PXE pxe;

    Dispatcher d{&pxe, 128};



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




void SetProgram(){
    RenderWorker::Program = [](int x, int y) -> Px{
       int centerX = WIDTH / 2;
       int centerY = HEIGHT / 2;
       float diffX = centerX - x;
       float diffY = centerY - y;
       float dist = sqrt((diffX * diffX) + (diffY * diffY));
       Px col = dist < 30 ? Px{255,static_cast<unsigned char>(dist * 8),static_cast<unsigned char>(dist * 4)} : Px{0,static_cast<unsigned char>(y),static_cast<unsigned char>(x)};
       return col;
        return dist < 30 ? Px{255,static_cast<unsigned char>(dist * 8),static_cast<unsigned char>(dist * 4)} : Px{static_cast<unsigned char>(rand()%256),static_cast<unsigned char>(rand()%256),0};

    };
}



#define SPXE_APPLICATION
#include "../spxe.h"
#include <ctime>
#include <iostream>
#include <functional>
#include <cmath>
#define SIZE 256
#define WIDTH SIZE
#define HEIGHT SIZE

#define ABS(n) ((n) > 0 ? (n) : -(n))


class PXE{
private:

    Px _curColor{255,255,255,255};
    Px* buffer;
public:

    PXE(Px* buf){
        buffer = buf;
    }

    void drawPixel(int x, int y)
    {
        drawPixel(buffer, x, y);
    }

    void drawPixel(Px* buf, int x, int y)
    {
        if(x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
        buf[y * WIDTH + x] = this->_curColor;
    }

    void setColor(int r, int g, int b, int a){
        _curColor.r = r;
        _curColor.g = g;
        _curColor.b = b;
        _curColor.a = a;
    }

    void setColor(int r, int g, int b){
        setColor(r,g,b,255);
    }

    void drawLine(int x0, int y0, int x1, int y1)
    {
        const int dx = ABS(x1 - x0);
        const int dy = -ABS(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        
        int error = dx + dy;
        
        while (1) {
            drawPixel(x0, y0);
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

    void drawRect(int x0, int y0, int x1, int y1)
    {
        drawLine(x0, y0, x1, y0);
        drawLine(x0, y1, x1, y1);
        drawLine(x1, y0, x1, y1);
        drawLine(x0, y0, x0, y1);
    }

    void setBuffer(Px* buf){
        this->buffer = buf;
    }
};

struct Rectangle{
    int x, y, width, height;
};

class RenderWorker{
public:
    static std::function<Px(int, int)> Program;
    PXE* pxe;
    Rectangle _workArea;

private:
    int _currentPixel;
    int _numPixelsCovered;
    bool _finished;

public:
    RenderWorker(PXE* pxe, int x, int y, int width, int height){
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
        pxe->setColor(255,255,0);
        pxe->drawRect(_workArea.x - 1, _workArea.y - 1, _workArea.x + _workArea.width, _workArea.y + _workArea.height);
    }

    bool IsFinished(){
        return _finished;
    }

    void Work(){
        if(_currentPixel < _numPixelsCovered){
            int localX = _currentPixel % _workArea.width;
            int localY = _currentPixel / _workArea.width;
            Px col = RenderWorker::Program(_workArea.x + localX, _workArea.y + localY);
            pxe->setColor(col.r, col.g, col.b);
            pxe->drawPixel(_workArea.x + localX, _workArea.y + localY);

            _currentPixel++;
        }else{
            _finished = true;
        }
    }
};

std::function<Px(int,int)> RenderWorker::Program;

void paintBuffer(Px* src, Px* target){
    memcpy(target, src, sizeof(Px) * WIDTH * HEIGHT);
}

void SetProgram();

int main(void)
{
    SetProgram();
    srand(time(nullptr));

    Px* pixbuf = spxeStart("spxe", 512, 512, WIDTH, HEIGHT);

    //Px* imgBuf = (Px*)malloc(WIDTH * HEIGHT * sizeof(Px));
    Px* imgBuf = new Px[WIDTH * HEIGHT]{};

    PXE pxe(imgBuf);

    


    int rwWidth = 16;
    int rwHeight = 16;

    int rwXJobs = WIDTH / rwHeight;
    int rwYJobs = HEIGHT / rwHeight;

    int rwX = 0;
    int rwY = rwYJobs - 1;

    RenderWorker rw = RenderWorker(&pxe, rwX * rwWidth, rwY * rwHeight, rwWidth,rwHeight);

    bool renderComplete = false;


    while (spxeRun(pixbuf)) {
        if (spxeKeyPressed(ESCAPE)) {
            break;
        }
        pxe.setBuffer(imgBuf);

        if(!renderComplete)
            rw.Work();

        paintBuffer(imgBuf, pixbuf);
        pxe.setBuffer(pixbuf);
        rw.DrawOutline();
        

        if(rw.IsFinished()){
            if(rwX < rwXJobs - 1){
                rwX++;
            }else{
                rwX = 0;
                rwY--;
                if(rwY < 0){
                    renderComplete = true;
                    std::cout << "Render Complete" << std::endl;
                    glfwSwapInterval(1);

                }
            }
                rw.Reset(rwX * rwWidth, rwY * rwHeight);
        }
    }

    delete[] imgBuf;
    return spxeEnd(pixbuf);
}

void SetProgram(){
    RenderWorker::Program = [](int x, int y) -> Px{
        // int centerX = WIDTH / 2;
        // int centerY = HEIGHT / 2;
        // float diffX = centerX - x;
        // float diffY = centerY - y;
        // float dist = sqrt((diffX * diffX) + (diffY * diffY));
        // Px col = dist < 30 ? Px{255,dist * 8,dist * 4} : Px{0,y,x};
        // return col;

        float r = double(x) / (WIDTH);
        float g = double(y) / (HEIGHT);
        float b = 0.25f;
        return Px{r * 256, g * 256, b * 256};
    };
}

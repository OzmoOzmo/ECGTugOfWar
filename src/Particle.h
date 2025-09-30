#pragma once

#include "Arduino.h"
#define FRICTION 1

class Particle
{
  public:
    void Spawn(int pos);
    void Tick();
    void Kill();
    bool Alive();
    int _posLed; // LED index
    int _power;
  private:
    int _pos;
    int _life;
    int _alive;
    int _sp;
};

void Particle::Spawn(int pos){
    //_pos = pos;
    _pos = map(pos, 0, NUM_LEDS - 1,0, 1000);
    _sp = random(-200, 200);
    _power = 255;
    _alive = 1;
    _life = 220 - abs(_sp);
}

void Particle::Tick(){
    if(_alive){
        _life ++;
        if(_sp > 0){
            _sp -= _life/10;
        }else{
            _sp += _life/10;
        }
        _power = 100 - _life;
        if(_power <= 0){
            Kill(); 
        }else{
            _pos += _sp/7.0;
            if(_pos > 1000){
                _pos = 1000;
                _sp = 0-(_sp/2);
            }
            else if(_pos < 0){
                _pos = 0;
                _sp = 0-(_sp/2);
            }
        }

        _posLed = map(_pos, 0, 1000, 0, NUM_LEDS - 1);
    }
}

bool Particle::Alive(){
    return _alive;
}

void Particle::Kill(){
    _alive = 0;
}

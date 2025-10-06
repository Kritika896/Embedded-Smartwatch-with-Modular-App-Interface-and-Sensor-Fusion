#ifndef MODEL_HPP
#define MODEL_HPP
#include <cstdint>

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();
    void setStepCount(uint16_t count);
protected:
    ModelListener* modelListener;
};

#endif // MODEL_HPP

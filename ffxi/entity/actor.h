#pragma once
#include "engine/entity/deformable_entity.h"

namespace FFXI {
    class SK2;
    class OS2;
}

//main FFXI entity class
class Actor : public lotus::DeformableEntity
{
public:
    explicit Actor(lotus::Engine* engine);
    void Init(const std::shared_ptr<Actor>& sp, const std::string& dat);

    float speed{ 4.f };
};

class FFXIActorLoader : public lotus::ModelLoader
{
public:
    FFXIActorLoader(const std::vector<FFXI::OS2*>& os2s, FFXI::SK2* sk2);
    virtual void LoadModel(std::shared_ptr<lotus::Model>&) override;
private:
    const std::vector<FFXI::OS2*>& os2s;
    FFXI::SK2* sk2;
};

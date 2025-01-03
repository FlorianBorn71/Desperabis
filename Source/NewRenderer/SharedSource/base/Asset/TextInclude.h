#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>

class D_IMPEXP TextInclude : public AssetBase
{
public:
    TextInclude() = default;
    virtual ~TextInclude() = default;

    virtual void Read(AssetStream& stream) override;
public:
    std::string m_text;
};


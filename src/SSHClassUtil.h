#pragma once
// 宏说明：
// PARAM_TYPE：变量类型（如 int）
// PARAM_NAME：变量名（如 _panelSeqId）
// 自动生成：Get_PARAM_NAME() / Set_PARAM_NAME(类型)(如Get_panelSeqId/Set_panelSeqId)
// 自动生成：protected
#define GEN_GET_SET_Protected(PARAM_TYPE, PARAM_NAME) \
public: \
    PARAM_TYPE Get##PARAM_NAME() const \
    { \
        return PARAM_NAME; \
    } \
    void Set##PARAM_NAME(PARAM_TYPE val) \
    { \
        PARAM_NAME = val; \
    }\
protected: \
    PARAM_TYPE PARAM_NAME;

// 宏说明：
// PARAM_TYPE：变量类型（如 int）
// PARAM_NAME：变量名（如 _panelSeqId）
// 自动生成：Get_PARAM_NAME() / Set_PARAM_NAME(类型)(如Get_panelSeqId/Set_panelSeqId)
// 自动生成：public
#define GEN_GET_SET_Private(PARAM_TYPE, PARAM_NAME) \
public: \
    PARAM_TYPE Get##PARAM_NAME() const \
    { \
        return PARAM_NAME; \
    } \
    void Set##PARAM_NAME(PARAM_TYPE val) \
    { \
        PARAM_NAME = val; \
    }\
private: \
    PARAM_TYPE PARAM_NAME;
// 宏说明：
// PARAM_TYPE：变量类型（如 int）
// PARAM_NAME：变量名（如 _panelSeqId）
// 自动生成：Get_PARAM_NAME() / Set_PARAM_NAME(类型)(如 Get_panelSeqId/Set_panelSeqId)
// 自动生成：仅仅Getting/Setting
#define GEN_GET_SET_ONLY(PARAM_TYPE, PARAM_NAME) \
public: \
    PARAM_TYPE Get##PARAM_NAME() const \
    { \
        return PARAM_NAME; \
    } \
    void Set##PARAM_NAME(PARAM_TYPE val) \
    { \
        PARAM_NAME = val; \
    }
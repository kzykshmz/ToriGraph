/***************************************************************************
    File                 : SafeLinearScaleEngine.h
    Project              : ToriGraph
    Description          : Internal helpers for ToriGraph's overflow-safe
                           QwtLinearScaleEngine subclass (implemented in Plot.cpp).
 ***************************************************************************/
#ifndef SAFE_LINEAR_SCALE_ENGINE_H
#define SAFE_LINEAR_SCALE_ENGINE_H

class QwtLinearScaleEngine;
class QwtScaleEngine;

//! Creates a QwtLinearScaleEngine subclass that guards against subnormal/non-finite
//! scale spans and extreme stepSize values that would otherwise trigger qRound()
//! signed integer overflow UB inside Qwt5.
QwtLinearScaleEngine *createSafeLinearScaleEngine();

//! Returns true for QwtLinearScaleEngine or ToriGraph's SafeLinearScaleEngine,
//! and false for specialized subclasses (FormulaLinkedScaleEngine, PiecewiseBreakScaleEngine).
bool isStandardLinearScaleEngine(const QwtScaleEngine *se);

#endif // SAFE_LINEAR_SCALE_ENGINE_H

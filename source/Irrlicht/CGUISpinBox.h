// Copyright (C) 2006-2012 Michael Zeilfelder
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_GUI_SPIN_BOX_H_INCLUDED__
#define __C_GUI_SPIN_BOX_H_INCLUDED__

#include "IrrCompileConfig.h"
#ifdef _IRR_COMPILE_WITH_GUI_

#include "IGUISpinBox.h"

namespace irr
{
namespace gui
{
	class IGUIEditBox;
	class IGUIButton;

	class CGUISpinBox : public IGUISpinBox
	{
	public:

		//! constructor
		CGUISpinBox(const wchar_t* text, bool border, IGUIEnvironment* environment,
			IGUIElement* parent, s32 id, const core::rect<s32>& rectangle);

		//! destructor
		virtual ~CGUISpinBox();

		//! Access the edit box used in the spin control
		/** \param enable: If set to true, the override color, which can be set
		with IGUIEditBox::setOverrideColor is used, otherwise the
		EGDC_BUTTON_TEXT color of the skin. */
		virtual IGUIEditBox* getEditBox() const _IRR_OVERRIDE_;

		//! set the current value of the spinbox
		/** \param val: value to be set in the spinbox */
		virtual void setValue(f32 val) _IRR_OVERRIDE_;

		//! Get the current value of the spinbox
		virtual f32 getValue() const _IRR_OVERRIDE_;

		//! set the range of values which can be used in the spinbox
		/** \param min: minimum value
		\param max: maximum value */
		virtual void setRange(f32 min, f32 max) _IRR_OVERRIDE_;

		//! get the minimum value which can be used in the spinbox
		virtual f32 getMin() const _IRR_OVERRIDE_;

		//! get the maximum value which can be used in the spinbox
		virtual f32 getMax() const _IRR_OVERRIDE_;

		//! step size by which values are changed when pressing the spin buttons
		/** \param step: stepsize used for value changes when pressing spin buttons */
		virtual void setStepSize(f32 step=1.f) _IRR_OVERRIDE_;

		//! returns the step size
		virtual f32 getStepSize() const _IRR_OVERRIDE_;

		//! called if an event happened.
		virtual bool OnEvent(const SEvent& event) _IRR_OVERRIDE_;

		//! Draws the element and its children.
		virtual void draw() _IRR_OVERRIDE_;

		//! Sets the new caption of the element
		virtual void setText(const wchar_t* text) _IRR_OVERRIDE_;

		//! Returns caption of this element.
		virtual const wchar_t* getText() const _IRR_OVERRIDE_;

		//! Sets the number of decimal places to display.
		//! Note that this also rounds the range to the same number of decimal places.
		/** \param places: The number of decimal places to display, use -1 to reset */
		virtual void setDecimalPlaces(s32 places) _IRR_OVERRIDE_;

		//! Sets when the spinbox has to validate entered text.
		/** \param validateOn Can be any combination of EGUI_SPINBOX_VALIDATION bit flags */
		virtual void setValidateOn(u32 validateOn) _IRR_OVERRIDE_;

		//! Gets when the spinbox has to validate entered text.
		virtual u32 getValidateOn() const _IRR_OVERRIDE_;

	protected:
		virtual void verifyValueRange();
		void refreshSprites();

		IGUIEditBox * EditBox;
		IGUIButton * ButtonSpinUp;
		IGUIButton * ButtonSpinDown;
		video::SColor CurrentIconColor;
		f32 StepSize;
		f32 RangeMin;
		f32 RangeMax;

		core::stringw FormatString;
		s32 DecimalPlaces;
		u32 ValidateOn;	// combination of EGUI_SPINBOX_VALIDATION bit-flags
	};


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif // __C_GUI_SPIN_BOX_H_INCLUDED__


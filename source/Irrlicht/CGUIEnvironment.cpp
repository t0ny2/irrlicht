
// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIEnvironment.h"

#ifdef _IRR_COMPILE_WITH_GUI_

#include "IVideoDriver.h"

#include "CGUISkin.h"
#include "CGUIButton.h"
#include "CGUIWindow.h"
#include "CGUIScrollBar.h"
#include "CGUIFont.h"
#include "CGUISpriteBank.h"
#include "CGUIImage.h"
#include "CGUIMeshViewer.h"
#include "CGUICheckBox.h"
#include "CGUIListBox.h"
#include "CGUITreeView.h"
#include "CGUIImageList.h"
#include "CGUIFileOpenDialog.h"
#include "CGUIColorSelectDialog.h"
#include "CGUIStaticText.h"
#include "CGUIEditBox.h"
#include "CGUISpinBox.h"
#include "CGUIInOutFader.h"
#include "CGUIMessageBox.h"
#include "CGUIModalScreen.h"
#include "CGUITabControl.h"
#include "CGUIContextMenu.h"
#include "CGUIComboBox.h"
#include "CGUIMenu.h"
#include "CGUIToolBar.h"
#include "CGUITable.h"
#include "CGUIProfiler.h"

#include "CDefaultGUIElementFactory.h"
#include "IWriteFile.h"

#include "BuiltInFont.h"
#include "os.h"

namespace irr
{
namespace gui
{

const io::path CGUIEnvironment::DefaultFontName = "#DefaultFont";

//! constructor
CGUIEnvironment::CGUIEnvironment(io::IFileSystem* fs, video::IVideoDriver* driver, IOSOperator* op)
: IGUIElement(EGUIET_ROOT, 0, 0, 0, core::rect<s32>(driver ? core::dimension2d<s32>(driver->getScreenSize()) : core::dimension2d<s32>(0,0))),
	Driver(driver), Hovered(0), HoveredNoSubelement(0), Focus(0), LastHoveredMousePos(0,0), CurrentSkin(0),
	FileSystem(fs), UserReceiver(0), Operator(op), FocusFlags(EFF_SET_ON_LMOUSE_DOWN|EFF_SET_ON_TAB)
{
	if (Driver)
		Driver->grab();

	if (FileSystem)
		FileSystem->grab();

	if (Operator)
		Operator->grab();

	#ifdef _DEBUG
	IGUIEnvironment::setDebugName("CGUIEnvironment");
	#endif

	// gui factory
	IGUIElementFactory* factory = new CDefaultGUIElementFactory(this);
	registerGUIElementFactory(factory);
	factory->drop();

	loadBuiltInFont();

	IGUISkin* skin = createSkin( gui::EGST_WINDOWS_METALLIC );
	setSkin(skin);
	skin->drop();

	//set tooltip default
	ToolTip.LastTime = 0;
	ToolTip.EnterTime = 0;
	ToolTip.LaunchTime = 1000;
	ToolTip.RelaunchTime = 500;
	ToolTip.Element = 0;

	// environment is root tab group
	Environment = this;
	setTabGroup(true);
}


//! destructor
CGUIEnvironment::~CGUIEnvironment()
{
	clearDeletionQueue();

	if ( HoveredNoSubelement && HoveredNoSubelement != this )
	{
		HoveredNoSubelement->drop();
		HoveredNoSubelement = 0;
	}

	if (Hovered && Hovered != this)
	{
		Hovered->drop();
		Hovered = 0;
	}

	if (Focus)
	{
		Focus->drop();
		Focus = 0;
	}

	if (ToolTip.Element)
	{
		ToolTip.Element->drop();
		ToolTip.Element = 0;
	}

	// drop skin
	if (CurrentSkin)
	{
		CurrentSkin->drop();
		CurrentSkin = 0;
	}

	u32 i;

	// delete all sprite banks
	for (i=0; i<Banks.size(); ++i)
		if (Banks[i].Bank)
			Banks[i].Bank->drop();

	// delete all fonts
	for (i=0; i<Fonts.size(); ++i)
		Fonts[i].Font->drop();

	// remove all factories
	for (i=0; i<GUIElementFactoryList.size(); ++i)
		GUIElementFactoryList[i]->drop();

	if (Operator)
	{
		Operator->drop();
		Operator = 0;
	}

	if (FileSystem)
	{
		FileSystem->drop();
		FileSystem = 0;
	}

	if (Driver)
	{
		Driver->drop();
		Driver = 0;
	}
}


void CGUIEnvironment::loadBuiltInFont()
{
	io::IReadFile* file = FileSystem->createMemoryReadFile(BuiltInFontData,
				BuiltInFontDataSize, DefaultFontName, false);

	CGUIFont* font = new CGUIFont(this, DefaultFontName );
	if (!font->load(file))
	{
		os::Printer::log("Error: Could not load built-in Font. Did you compile without the BMP loader?", ELL_ERROR);
		font->drop();
		file->drop();
		return;
	}

	SFont f;
	f.NamedPath.setPath(DefaultFontName);
	f.Font = font;
	Fonts.push_back(f);

	file->drop();
}


//! draws all gui elements
void CGUIEnvironment::drawAll(bool useScreenSize)
{
	if (useScreenSize && Driver)
	{
		core::dimension2d<s32> dim(Driver->getScreenSize());
		if (AbsoluteRect.LowerRightCorner.X != dim.Width ||
			AbsoluteRect.UpperLeftCorner.X != 0 ||
			AbsoluteRect.LowerRightCorner.Y != dim.Height ||
			AbsoluteRect.UpperLeftCorner.Y != 0
			)
		{
			setRelativePosition(core::recti(0,0,dim.Width, dim.Height));
		}
	}

	// make sure tooltip is always on top
	if (ToolTip.Element)
		bringToFront(ToolTip.Element);

	draw();
	OnPostRender ( os::Timer::getTime () );

	clearDeletionQueue();
}


//! sets the focus to an element
bool CGUIEnvironment::setFocus(IGUIElement* element)
{
	if (Focus == element)
	{
		return false;
	}

	// GUI Environment should just reset the focus to 0
	if (element == this)
		element = 0;

	// stop element from being deleted
	if (element)
		element->grab();

	// focus may change or be removed in this call
	IGUIElement *currentFocus = 0;
	if (Focus)
	{
		currentFocus = Focus;
		currentFocus->grab();
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = Focus;
		e.GUIEvent.Element = element;
		e.GUIEvent.EventType = EGET_ELEMENT_FOCUS_LOST;
		if (Focus->OnEvent(e))
		{
			if (element)
				element->drop();
			currentFocus->drop();
			return false;
		}
		currentFocus->drop();
		currentFocus = 0;
	}

	if (element)
	{
		currentFocus = Focus;
		if (currentFocus)
			currentFocus->grab();

		// send focused event
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = element;
		e.GUIEvent.Element = Focus;
		e.GUIEvent.EventType = EGET_ELEMENT_FOCUSED;
		if (element->OnEvent(e))
		{
			if (element)
				element->drop();
			if (currentFocus)
				currentFocus->drop();
			return false;
		}
	}

	if (currentFocus)
		currentFocus->drop();

	if (Focus)
		Focus->drop();

	// element is the new focus so it doesn't have to be dropped
	Focus = element;

	return true;
}


//! returns the element with the focus
IGUIElement* CGUIEnvironment::getFocus() const
{
	return Focus;
}

//! returns the element last known to be under the mouse cursor
IGUIElement* CGUIEnvironment::getHovered() const
{
	return Hovered;
}


//! removes the focus from an element
bool CGUIEnvironment::removeFocus(IGUIElement* element)
{
	if (Focus && Focus==element)
	{
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = Focus;
		e.GUIEvent.Element = 0;
		e.GUIEvent.EventType = EGET_ELEMENT_FOCUS_LOST;
		if (Focus->OnEvent(e))
		{
			return false;
		}
	}
	if (Focus)
	{
		Focus->drop();
		Focus = 0;
	}

	return true;
}


//! Returns whether the element has focus
bool CGUIEnvironment::hasFocus(const IGUIElement* element, bool checkSubElements) const
{
	if (element == Focus)
		return true;

	if ( !checkSubElements || !element )
		return false;

	IGUIElement* f = Focus;
	while ( f && f->isSubElement() )
	{
		f = f->getParent();
		if ( f == element )
			return true;
	}
	return false;
}


//! returns the current video driver
video::IVideoDriver* CGUIEnvironment::getVideoDriver() const
{
	return Driver;
}


//! returns the current file system
io::IFileSystem* CGUIEnvironment::getFileSystem() const
{
	return FileSystem;
}


//! returns a pointer to the OS operator
IOSOperator* CGUIEnvironment::getOSOperator() const
{
	return Operator;
}


//! clear all GUI elements
void CGUIEnvironment::clear()
{
	// Remove the focus
	if (Focus)
	{
		Focus->drop();
		Focus = 0;
	}

	if (Hovered && Hovered != this)
	{
		Hovered->drop();
		Hovered = 0;
	}
	if ( HoveredNoSubelement && HoveredNoSubelement != this)
	{
		HoveredNoSubelement->drop();
		HoveredNoSubelement = 0;
	}

	// get the root's children in case the root changes in future
	const core::list<IGUIElement*>& children = getRootGUIElement()->getChildren();

	while (!children.empty())
		(*children.getLast())->remove();
}


//! called by ui if an event happened.
bool CGUIEnvironment::OnEvent(const SEvent& event)
{

	bool ret = false;
	if (UserReceiver
		&& (event.EventType != EET_MOUSE_INPUT_EVENT)
		&& (event.EventType != EET_KEY_INPUT_EVENT)
		&& (event.EventType != EET_GUI_EVENT || event.GUIEvent.Caller != this))
	{
		ret = UserReceiver->OnEvent(event);
	}

	return ret;
}

//
void CGUIEnvironment::OnPostRender( u32 time )
{
	// launch tooltip
	if ( ToolTip.Element == 0 &&
		HoveredNoSubelement && HoveredNoSubelement != this &&
		(time - ToolTip.EnterTime >= ToolTip.LaunchTime
		|| (time - ToolTip.LastTime >= ToolTip.RelaunchTime && time - ToolTip.LastTime < ToolTip.LaunchTime)) &&
		HoveredNoSubelement->getToolTipText().size() &&
		getSkin() &&
		getSkin()->getFont(EGDF_TOOLTIP)
		)
	{
		core::rect<s32> pos;

		pos.UpperLeftCorner = LastHoveredMousePos;
		core::dimension2du dim = getSkin()->getFont(EGDF_TOOLTIP)->getDimension(HoveredNoSubelement->getToolTipText().c_str());
		dim.Width += getSkin()->getSize(EGDS_TEXT_DISTANCE_X)*2;
		dim.Height += getSkin()->getSize(EGDS_TEXT_DISTANCE_Y)*2;

		pos.UpperLeftCorner.Y -= dim.Height+1;
		pos.LowerRightCorner.Y = pos.UpperLeftCorner.Y + dim.Height-1;
		pos.LowerRightCorner.X = pos.UpperLeftCorner.X + dim.Width;

		pos.constrainTo(getAbsolutePosition());

		ToolTip.Element = addStaticText(HoveredNoSubelement->getToolTipText().c_str(), pos, true, true, this, -1, true);
		ToolTip.Element->setOverrideColor(getSkin()->getColor(EGDC_TOOLTIP));
		ToolTip.Element->setBackgroundColor(getSkin()->getColor(EGDC_TOOLTIP_BACKGROUND));
		ToolTip.Element->setOverrideFont(getSkin()->getFont(EGDF_TOOLTIP));
		ToolTip.Element->setSubElement(true);
		ToolTip.Element->grab();

		s32 textHeight = ToolTip.Element->getTextHeight();
		pos = ToolTip.Element->getRelativePosition();
		pos.LowerRightCorner.Y = pos.UpperLeftCorner.Y + textHeight;
		ToolTip.Element->setRelativePosition(pos);
	}

	if (ToolTip.Element && ToolTip.Element->isVisible() )	// (isVisible() check only because we might use visibility for ToolTip one day)
	{
		ToolTip.LastTime = time;

		// got invisible or removed in the meantime?
		if ( !HoveredNoSubelement ||
			!HoveredNoSubelement->isVisible() ||
			!HoveredNoSubelement->getParent()
			)	// got invisible or removed in the meantime?
		{
			ToolTip.Element->remove();
			ToolTip.Element->drop();
			ToolTip.Element = 0;
		}
	}

	IGUIElement::OnPostRender ( time );
}

void CGUIEnvironment::addToDeletionQueue(IGUIElement* element)
{
	if (!element)
		return;

	element->grab();
	DeletionQueue.push_back(element);
}

void CGUIEnvironment::clearDeletionQueue()
{
	if (DeletionQueue.empty())
		return;

	for (u32 i=0; i<DeletionQueue.size(); ++i)
	{
		DeletionQueue[i]->remove();
		DeletionQueue[i]->drop();
	}

	DeletionQueue.clear();
}

//
void CGUIEnvironment::updateHoveredElement(core::position2d<s32> mousePos)
{
	IGUIElement* lastHovered = Hovered;
	IGUIElement* lastHoveredNoSubelement = HoveredNoSubelement;
	LastHoveredMousePos = mousePos;

	Hovered = getElementFromPoint(mousePos);

	if ( ToolTip.Element && Hovered == ToolTip.Element )
	{
		// When the mouse is over the ToolTip we remove that so it will be re-created at a new position.
		// Note that ToolTip.EnterTime does not get changed here, so it will be re-created at once.
		ToolTip.Element->remove();
		ToolTip.Element->drop();
		ToolTip.Element = 0;

		// Get the real Hovered
		Hovered = getElementFromPoint(mousePos);
	}

	// for tooltips we want the element itself and not some of it's subelements
	HoveredNoSubelement = Hovered;
	while ( HoveredNoSubelement && HoveredNoSubelement->isSubElement() )
	{
		HoveredNoSubelement = HoveredNoSubelement->getParent();
	}

	if (Hovered && Hovered != this)
		Hovered->grab();
	if ( HoveredNoSubelement && HoveredNoSubelement != this)
		HoveredNoSubelement->grab();

	if (Hovered != lastHovered)
	{
		SEvent event;
		event.EventType = EET_GUI_EVENT;

		if (lastHovered)
		{
			event.GUIEvent.Caller = lastHovered;
			event.GUIEvent.Element = 0;
			event.GUIEvent.EventType = EGET_ELEMENT_LEFT;
			lastHovered->OnEvent(event);
		}

		if ( Hovered )
		{
			event.GUIEvent.Caller  = Hovered;
			event.GUIEvent.Element = Hovered;
			event.GUIEvent.EventType = EGET_ELEMENT_HOVERED;
			Hovered->OnEvent(event);
		}
	}

	if ( lastHoveredNoSubelement != HoveredNoSubelement )
	{
		if (ToolTip.Element)
		{
			ToolTip.Element->remove();
			ToolTip.Element->drop();
			ToolTip.Element = 0;
		}

		if ( HoveredNoSubelement )
		{
			u32 now = os::Timer::getTime();
			ToolTip.EnterTime = now;
		}
	}

	if (lastHovered && lastHovered != this)
		lastHovered->drop();
	if (lastHoveredNoSubelement && lastHoveredNoSubelement != this)
		lastHoveredNoSubelement->drop();
}


//! This sets a new event receiver for gui events. Usually you do not have to
//! use this method, it is used by the internal engine.
void CGUIEnvironment::setUserEventReceiver(IEventReceiver* evr)
{
	UserReceiver = evr;
}


//! posts an input event to the environment
bool CGUIEnvironment::postEventFromUser(const SEvent& event)
{
	switch(event.EventType)
	{
	case EET_GUI_EVENT:
		{
			// hey, why is the user sending gui events..?
		}

		break;
	case EET_MOUSE_INPUT_EVENT:

		updateHoveredElement(core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y));

		if ( Hovered != Focus )
		{
			IGUIElement * focusCandidate = Hovered;

			// Only allow enabled elements to be focused (unless EFF_CAN_FOCUS_DISABLED is set)
			if ( Hovered && !Hovered->isEnabled() && !(FocusFlags & EFF_CAN_FOCUS_DISABLED))
				focusCandidate = NULL;	// we still remove focus from the active element

			// Please don't merge this into a single if clause, it's easier to debug the way it is
			if (FocusFlags & EFF_SET_ON_LMOUSE_DOWN &&
				event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN )
			{
				setFocus(focusCandidate);
			}
			else if ( FocusFlags & EFF_SET_ON_RMOUSE_DOWN &&
				event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN )
			{
				setFocus(focusCandidate);
			}
			else if ( FocusFlags & EFF_SET_ON_MOUSE_OVER &&
				event.MouseInput.Event == EMIE_MOUSE_MOVED )
			{
				setFocus(focusCandidate);
			}
		}

		// sending input to focus
		if (Focus && Focus->OnEvent(event))
			return true;

		// focus could have died in last call
		if (!Focus && Hovered)
		{
			return Hovered->OnEvent(event);
		}

		break;
	case EET_KEY_INPUT_EVENT:
		{
			if (Focus && Focus->OnEvent(event))
				return true;

			// For keys we handle the event before changing focus to give elements the chance for catching the TAB
			// Send focus changing event
			// CAREFUL when changing - there's an identical check in CGUIModalScreen::OnEvent
			if (FocusFlags & EFF_SET_ON_TAB &&
				event.KeyInput.PressedDown &&
				event.KeyInput.Key == KEY_TAB)
			{
				IGUIElement *next = getNextElement(event.KeyInput.Shift, event.KeyInput.Control);
				if (next && next != Focus)
				{
					if (setFocus(next))
						return true;
				}
			}
		}
		break;
	case EET_STRING_INPUT_EVENT:
		if (Focus && Focus->OnEvent(event))
			return true;
		break;
	default:
		break;
	} // end switch

	return false;
}


//! returns the current gui skin
IGUISkin* CGUIEnvironment::getSkin() const
{
	return CurrentSkin;
}


//! Sets a new GUI Skin
void CGUIEnvironment::setSkin(IGUISkin* skin)
{
	if (CurrentSkin==skin)
		return;

	if (CurrentSkin)
		CurrentSkin->drop();

	CurrentSkin = skin;

	if (CurrentSkin)
		CurrentSkin->grab();
}


//! Creates a new GUI Skin based on a template.
/** \return Returns a pointer to the created skin.
If you no longer need the skin, you should call IGUISkin::drop().
See IReferenceCounted::drop() for more information. */
IGUISkin* CGUIEnvironment::createSkin(EGUI_SKIN_TYPE type)
{
	IGUISkin* skin = new CGUISkin(type, Driver);

	IGUIFont* builtinfont = getBuiltInFont();
	IGUIFontBitmap* bitfont = 0;
	if (builtinfont && builtinfont->getType() == EGFT_BITMAP)
		bitfont = (IGUIFontBitmap*)builtinfont;

	IGUISpriteBank* bank = 0;
	skin->setFont(builtinfont);

	if (bitfont)
		bank = bitfont->getSpriteBank();

	skin->setSpriteBank(bank);

	return skin;
}


//! Returns the default element factory which can create all built in elements
IGUIElementFactory* CGUIEnvironment::getDefaultGUIElementFactory() const
{
	return getGUIElementFactory(0);
}


//! Adds an element factory to the gui environment.
/** Use this to extend the gui environment with new element types which it should be
able to create automatically, for example when loading data from xml files. */
void CGUIEnvironment::registerGUIElementFactory(IGUIElementFactory* factoryToAdd)
{
	if (factoryToAdd)
	{
		factoryToAdd->grab();
		GUIElementFactoryList.push_back(factoryToAdd);
	}
}


//! Returns amount of registered scene node factories.
u32 CGUIEnvironment::getRegisteredGUIElementFactoryCount() const
{
	return GUIElementFactoryList.size();
}


//! Returns a scene node factory by index
IGUIElementFactory* CGUIEnvironment::getGUIElementFactory(u32 index) const
{
	if (index < GUIElementFactoryList.size())
		return GUIElementFactoryList[index];
	else
		return 0;
}


//! adds a GUI Element using its name
IGUIElement* CGUIEnvironment::addGUIElement(const c8* elementName, IGUIElement* parent)
{
	IGUIElement* node=0;

	if (!parent)
		parent = this;

	for (s32 i=GUIElementFactoryList.size()-1; i>=0 && !node; --i)
		node = GUIElementFactoryList[i]->addGUIElement(elementName, parent);


	return node;
}


//! Saves the current gui into a file.
//! \param filename: Name of the file .
bool CGUIEnvironment::saveGUI(const io::path& filename, IGUIElement* start)
{
	io::IWriteFile* file = FileSystem->createAndWriteFile(filename);
	if (!file)
	{
		return false;
	}

	bool ret = saveGUI(file, start);
	file->drop();
	return ret;
}


//! Saves the current gui into a file.
bool CGUIEnvironment::saveGUI(io::IWriteFile* file, IGUIElement* start)
{
	return false;
}


//! Loads the gui. Note that the current gui is not cleared before.
//! \param filename: Name of the file.
bool CGUIEnvironment::loadGUI(const io::path& filename, IGUIElement* parent)
{
	io::IReadFile* read = FileSystem->createAndOpenFile(filename);
	if (!read)
	{
		os::Printer::log("Unable to open gui file", filename, ELL_ERROR);
		return false;
	}

	bool ret = loadGUI(read, parent);
	read->drop();

	return ret;
}


//! Loads the gui. Note that the current gui is not cleared before.
bool CGUIEnvironment::loadGUI(io::IReadFile* file, IGUIElement* parent)
{
	return false;
}

//! adds a button. The returned pointer must not be dropped.
IGUIButton* CGUIEnvironment::addButton(const core::rect<s32>& rectangle, IGUIElement* parent, s32 id, const wchar_t* text, const wchar_t *tooltiptext)
{
	IGUIButton* button = new CGUIButton(this, parent ? parent : this, id, rectangle);
	if (text)
		button->setText(text);

	if ( tooltiptext )
		button->setToolTipText ( tooltiptext );

	button->drop();
	return button;
}


//! adds a window. The returned pointer must not be dropped.
IGUIWindow* CGUIEnvironment::addWindow(const core::rect<s32>& rectangle, bool modal,
		const wchar_t* text, IGUIElement* parent, s32 id)
{
	parent = parent ? parent : this;

	IGUIWindow* win = new CGUIWindow(this, parent, id, rectangle);
	if (text)
		win->setText(text);
	win->drop();

	if (modal)
	{
		// Careful, don't just set the modal as parent above. That will mess up the focus (and is hard to change because we have to be very
		// careful not to get virtual function call, like OnEvent, in the window.
		CGUIModalScreen * modalScreen = new CGUIModalScreen(this, parent, -1);
		modalScreen->drop();
		modalScreen->addChild(win);
	}

	return win;
}


//! adds a modal screen. The returned pointer must not be dropped.
IGUIElement* CGUIEnvironment::addModalScreen(IGUIElement* parent, int blinkMode)
{
	parent = parent ? parent : this;

	CGUIModalScreen *win = new CGUIModalScreen(this, parent, -1);
	win->setBlinkMode(blinkMode);
	win->drop();

	return win;
}


//! Adds a message box.
IGUIWindow* CGUIEnvironment::addMessageBox(const wchar_t* caption, const wchar_t* text,
	bool modal, s32 flag, IGUIElement* parent, s32 id, video::ITexture* image)
{
	if (!CurrentSkin)
		return 0;

	parent = parent ? parent : this;

	core::rect<s32> rect;
	core::dimension2d<u32> screenDim, msgBoxDim;

	screenDim.Width = parent->getAbsolutePosition().getWidth();
	screenDim.Height = parent->getAbsolutePosition().getHeight();
	msgBoxDim.Width = 2;
	msgBoxDim.Height = 2;

	rect.UpperLeftCorner.X = (screenDim.Width - msgBoxDim.Width) / 2;
	rect.UpperLeftCorner.Y = (screenDim.Height - msgBoxDim.Height) / 2;
	rect.LowerRightCorner.X = rect.UpperLeftCorner.X + msgBoxDim.Width;
	rect.LowerRightCorner.Y = rect.UpperLeftCorner.Y + msgBoxDim.Height;

	IGUIWindow* win = new CGUIMessageBox(this, caption, text, flag,
		parent, id, rect, image);
	win->drop();

	if (modal)
	{
		// Careful, don't just set the modal as parent above. That will mess up the focus (and is hard to change because we have to be very
		// careful not to get virtual function call, like OnEvent, in the CGUIMessageBox.
		CGUIModalScreen * modalScreen = new CGUIModalScreen(this, parent, -1);
		modalScreen->drop();
		modalScreen->addChild( win );
	}


	return win;
}


//! adds a scrollbar. The returned pointer must not be dropped.
IGUIScrollBar* CGUIEnvironment::addScrollBar(bool horizontal, const core::rect<s32>& rectangle, IGUIElement* parent, s32 id)
{
	IGUIScrollBar* bar = new CGUIScrollBar(horizontal, this, parent ? parent : this, id, rectangle);
	bar->drop();
	return bar;
}

//! Adds a table to the environment
IGUITable* CGUIEnvironment::addTable(const core::rect<s32>& rectangle, IGUIElement* parent, s32 id, bool drawBackground)
{
	CGUITable* b = new CGUITable(this, parent ? parent : this, id, rectangle, true, drawBackground, false);
	b->drop();
	return b;
}

	//! Adds an element to display the information from the Irrlicht profiler
IGUIProfiler* CGUIEnvironment::addProfilerDisplay(const core::rect<s32>& rectangle, IGUIElement* parent, s32 id)
{
	CGUIProfiler* p = new CGUIProfiler(this, parent ? parent : this, id, rectangle, NULL);
	p->drop();
	return p;
}

//! Adds an image element.
IGUIImage* CGUIEnvironment::addImage(video::ITexture* image, core::position2d<s32> pos,
	bool useAlphaChannel, IGUIElement* parent, s32 id, const wchar_t* text)
{
	core::dimension2d<s32> sz(0,0);
	if (image)
		sz = core::dimension2d<s32>(image->getOriginalSize());

	IGUIImage* img = new CGUIImage(this, parent ? parent : this,
		id, core::rect<s32>(pos, sz));

	if (text)
		img->setText(text);

	if (useAlphaChannel)
		img->setUseAlphaChannel(true);

	if (image)
		img->setImage(image);

	img->drop();
	return img;
}


//! adds an image. The returned pointer must not be dropped.
IGUIImage* CGUIEnvironment::addImage(const core::rect<s32>& rectangle, IGUIElement* parent, s32 id, const wchar_t* text, bool useAlphaChannel)
{
	IGUIImage* img = new CGUIImage(this, parent ? parent : this,
		id, rectangle);

	if (text)
		img->setText(text);

	if ( useAlphaChannel )
		img->setUseAlphaChannel(true);

	img->drop();
	return img;
}


//! adds an mesh viewer. The returned pointer must not be dropped.
IGUIMeshViewer* CGUIEnvironment::addMeshViewer(const core::rect<s32>& rectangle, IGUIElement* parent, s32 id, const wchar_t* text)
{
	IGUIMeshViewer* v = new CGUIMeshViewer(this, parent ? parent : this,
		id, rectangle);

	if (text)
		v->setText(text);

	v->drop();
	return v;
}


//! adds a checkbox
IGUICheckBox* CGUIEnvironment::addCheckBox(bool checked, const core::rect<s32>& rectangle, IGUIElement* parent, s32 id, const wchar_t* text)
{
	IGUICheckBox* b = new CGUICheckBox(checked, this,
		parent ? parent : this , id , rectangle);

	if (text)
		b->setText(text);

	b->drop();
	return b;
}


//! adds a list box
IGUIListBox* CGUIEnvironment::addListBox(const core::rect<s32>& rectangle,
					IGUIElement* parent, s32 id, bool drawBackground)
{
	IGUIListBox* b = new CGUIListBox(this, parent ? parent : this, id, rectangle,
		true, drawBackground, false);

	if (CurrentSkin && CurrentSkin->getSpriteBank())
	{
		b->setSpriteBank(CurrentSkin->getSpriteBank());
	}
	else if (getBuiltInFont() && getBuiltInFont()->getType() == EGFT_BITMAP)
	{
		b->setSpriteBank( ((IGUIFontBitmap*)getBuiltInFont())->getSpriteBank());
	}

	b->drop();
	return b;
}

//! adds a tree view
IGUITreeView* CGUIEnvironment::addTreeView(const core::rect<s32>& rectangle,
					 IGUIElement* parent, s32 id,
					 bool drawBackground,
					 bool scrollBarVertical, bool scrollBarHorizontal)
{
	IGUITreeView* b = new CGUITreeView(this, parent ? parent : this, id, rectangle,
		true, drawBackground, scrollBarVertical, scrollBarHorizontal);

	b->setIconFont ( getBuiltInFont () );
	b->drop();
	return b;
}

//! adds a file open dialog. The returned pointer must not be dropped.
IGUIFileOpenDialog* CGUIEnvironment::addFileOpenDialog(const wchar_t* title,
				bool modal, IGUIElement* parent, s32 id,
				bool restoreCWD, io::path::char_type* startDir)
{
	parent = parent ? parent : this;

	IGUIFileOpenDialog* d = new CGUIFileOpenDialog(title, this, parent, id,
			restoreCWD, startDir);
	d->drop();

	if (modal)
	{
		// Careful, don't just set the modal as parent above. That will mess up the focus (and is hard to change because we have to be very
		// careful not to get virtual function call, like OnEvent, in the window.
		CGUIModalScreen * modalScreen = new CGUIModalScreen(this, parent, -1);
		modalScreen->drop();
		modalScreen->addChild(d);
	}

	return d;
}


//! adds a color select dialog. The returned pointer must not be dropped.
IGUIColorSelectDialog* CGUIEnvironment::addColorSelectDialog(const wchar_t* title,
				bool modal, IGUIElement* parent, s32 id)
{
	parent = parent ? parent : this;

	IGUIColorSelectDialog* d = new CGUIColorSelectDialog( title,
			this, parent, id);
	d->drop();

	if (modal)
	{
		// Careful, don't just set the modal as parent above. That will mess up the focus (and is hard to change because we have to be very
		// careful not to get virtual function call, like OnEvent, in the window.
		CGUIModalScreen * modalScreen = new CGUIModalScreen(this, parent, -1);
		modalScreen->drop();
		modalScreen->addChild(d);
	}

	return d;
}


//! adds a static text. The returned pointer must not be dropped.
IGUIStaticText* CGUIEnvironment::addStaticText(const wchar_t* text,
				const core::rect<s32>& rectangle,
				bool border, bool wordWrap,
				IGUIElement* parent, s32 id, bool background)
{
	IGUIStaticText* d = new CGUIStaticText(text, border, this,
			parent ? parent : this, id, rectangle, background);

	d->setWordWrap(wordWrap);
	d->drop();

	return d;
}


//! Adds an edit box. The returned pointer must not be dropped.
IGUIEditBox* CGUIEnvironment::addEditBox(const wchar_t* text,
			const core::rect<s32>& rectangle, bool border,
			IGUIElement* parent, s32 id)
{
	IGUIEditBox* d = new CGUIEditBox(text, border, this,
			parent ? parent : this, id, rectangle);

	d->drop();
	return d;
}


//! Adds a spin box to the environment
IGUISpinBox* CGUIEnvironment::addSpinBox(const wchar_t* text,
					 const core::rect<s32> &rectangle,
					 bool border,IGUIElement* parent, s32 id)
{
	IGUISpinBox* d = new CGUISpinBox(text, border,this,
		parent ? parent : this, id, rectangle);

	d->drop();
	return d;
}


//! Adds a tab control to the environment.
IGUITabControl* CGUIEnvironment::addTabControl(const core::rect<s32>& rectangle,
	IGUIElement* parent, bool fillbackground, bool border, s32 id)
{
	IGUITabControl* t = new CGUITabControl(this, parent ? parent : this,
		rectangle, fillbackground, border, id);
	t->drop();
	return t;
}


//! Adds tab to the environment.
IGUITab* CGUIEnvironment::addTab(const core::rect<s32>& rectangle,
	IGUIElement* parent, s32 id)
{
	IGUITab* t = new CGUITab(this, parent ? parent : this,
		rectangle, id);
	t->drop();
	return t;
}


//! Adds a context menu to the environment.
IGUIContextMenu* CGUIEnvironment::addContextMenu(const core::rect<s32>& rectangle,
	IGUIElement* parent, s32 id)
{
	IGUIContextMenu* c = new CGUIContextMenu(this,
		parent ? parent : this, id, rectangle, true);
	c->drop();
	return c;
}


//! Adds a menu to the environment.
IGUIContextMenu* CGUIEnvironment::addMenu(IGUIElement* parent, s32 id)
{
	if (!parent)
		parent = this;

	IGUIContextMenu* c = new CGUIMenu(this,
		parent, id, core::rect<s32>(0,0,
				parent->getAbsolutePosition().getWidth(),
				parent->getAbsolutePosition().getHeight()));

	c->drop();
	return c;
}


//! Adds a toolbar to the environment. It is like a menu is always placed on top
//! in its parent, and contains buttons.
IGUIToolBar* CGUIEnvironment::addToolBar(IGUIElement* parent, s32 id)
{
	if (!parent)
		parent = this;

	IGUIToolBar* b = new CGUIToolBar(this, parent, id, core::rect<s32>(0,0,10,10));
	b->drop();
	return b;
}


//! Adds an element for fading in or out.
IGUIInOutFader* CGUIEnvironment::addInOutFader(const core::rect<s32>* rectangle, IGUIElement* parent, s32 id)
{
	core::rect<s32> rect;

	if (rectangle)
		rect = *rectangle;
	else if (Driver)
		rect = core::rect<s32>(core::dimension2di(Driver->getScreenSize()));

	if (!parent)
		parent = this;

	IGUIInOutFader* fader = new CGUIInOutFader(this, parent, id, rect);
	fader->drop();
	return fader;
}


//! Adds a combo box to the environment.
IGUIComboBox* CGUIEnvironment::addComboBox(const core::rect<s32>& rectangle,
	IGUIElement* parent, s32 id)
{
	IGUIComboBox* t = new CGUIComboBox(this, parent ? parent : this,
		id, rectangle);
	t->drop();
	return t;
}


//! returns the font
IGUIFont* CGUIEnvironment::getFont(const io::path& filename)
{
	// search existing font

	SFont f;
	f.NamedPath.setPath(filename);

	s32 index = Fonts.binary_search(f);
	if (index != -1)
		return Fonts[index].Font;

	// font doesn't exist, attempt to load it

	// does the file exist?

	if (!FileSystem->existFile(filename))
	{
		os::Printer::log("Could not load font because the file does not exist", f.NamedPath.getPath(), ELL_ERROR);
		return 0;
	}

	IGUIFont* ifont=0;
#if 0
		{
			CGUIFont* font = new CGUIFont(this, filename);
			ifont = (IGUIFont*)font;

			// load the font
			io::path directory;
			core::splitFilename(filename, &directory);
			if (!font->load(xml, directory))
			{
				font->drop();
				font  = 0;
				ifont = 0;
			}
		}
#endif


	if (!ifont)
	{

		CGUIFont* font = new CGUIFont(this, f.NamedPath.getPath() );
		ifont = (IGUIFont*)font;
		if (!font->load(f.NamedPath.getPath()))
		{
			font->drop();
			return 0;
		}
	}

	// add to fonts.

	f.Font = ifont;
	Fonts.push_back(f);

	return ifont;
}


//! add an externally loaded font
IGUIFont* CGUIEnvironment::addFont(const io::path& name, IGUIFont* font)
{
	if (font)
	{
		SFont f;
		f.NamedPath.setPath(name);
		s32 index = Fonts.binary_search(f);
		if (index != -1)
			return Fonts[index].Font;
		f.Font = font;
		Fonts.push_back(f);
		font->grab();
	}
	return font;
}

//! remove loaded font
void CGUIEnvironment::removeFont(IGUIFont* font)
{
	if ( !font )
		return;
	for ( u32 i=0; i<Fonts.size(); ++i )
	{
		if ( Fonts[i].Font == font )
		{
			Fonts[i].Font->drop();
			Fonts.erase(i);
			return;
		}
	}
}

//! returns default font
IGUIFont* CGUIEnvironment::getBuiltInFont() const
{
	if (Fonts.empty())
		return 0;

	return Fonts[0].Font;
}


IGUISpriteBank* CGUIEnvironment::getSpriteBank(const io::path& filename)
{
	// search for the file name

	SSpriteBank b;
	b.NamedPath.setPath(filename);

	s32 index = Banks.binary_search(b);
	if (index != -1)
		return Banks[index].Bank;

	// we don't have this sprite bank, we should load it
	if (!FileSystem->existFile(b.NamedPath.getPath()))
	{
		if ( filename != DefaultFontName )
		{
			os::Printer::log("Could not load sprite bank because the file does not exist", b.NamedPath.getPath(), ELL_DEBUG);
		}
		return 0;
	}

	// todo: load it!

	return 0;
}


IGUISpriteBank* CGUIEnvironment::addEmptySpriteBank(const io::path& name)
{
	// no duplicate names allowed

	SSpriteBank b;
	b.NamedPath.setPath(name);

	const s32 index = Banks.binary_search(b);
	if (index != -1)
		return 0;

	// create a new sprite bank

	b.Bank = new CGUISpriteBank(this);
	Banks.push_back(b);

	return b.Bank;
}


//! Creates the image list from the given texture.
IGUIImageList* CGUIEnvironment::createImageList(  video::ITexture* texture,
					core::dimension2d<s32>	imageSize, bool useAlphaChannel )
{
	CGUIImageList* imageList = new CGUIImageList( Driver );
	if( !imageList->createImageList( texture, imageSize, useAlphaChannel ) )
	{
		imageList->drop();
		return 0;
	}

	return imageList;
}

//! Returns the root gui element.
IGUIElement* CGUIEnvironment::getRootGUIElement()
{
	return this;
}


//! Returns the next element in the tab group starting at the focused element
IGUIElement* CGUIEnvironment::getNextElement(bool reverse, bool group)
{
	// start the search at the root of the current tab group
	IGUIElement *startPos = Focus ? Focus->getTabGroup() : 0;
	s32 startOrder = -1;

	// if we're searching for a group
	if (group && startPos)
	{
		startOrder = startPos->getTabOrder();
	}
	else
	if (!group && Focus && !Focus->isTabGroup())
	{
		startOrder = Focus->getTabOrder();
		if (startOrder == -1)
		{
			// this element is not part of the tab cycle,
			// but its parent might be...
			IGUIElement *el = Focus;
			while (el && el->getParent() && startOrder == -1)
			{
				el = el->getParent();
				startOrder = el->getTabOrder();
			}

		}
	}

	if (group || !startPos)
		startPos = this; // start at the root

	// find the element
	IGUIElement *closest = 0;
	IGUIElement *first = 0;
	startPos->getNextElement(startOrder, reverse, group, first, closest, false, (FocusFlags & EFF_CAN_FOCUS_DISABLED) != 0);

	if (closest)
		return closest; // we found an element
	else if (first)
		return first; // go to the end or the start
	else if (group)
		return this; // no group found? root group
	else
		return 0;
}

void CGUIEnvironment::setFocusBehavior(u32 flags)
{
	FocusFlags = flags;
}

u32 CGUIEnvironment::getFocusBehavior() const
{
	return FocusFlags;
}

//! creates an GUI Environment
IGUIEnvironment* createGUIEnvironment(io::IFileSystem* fs,
					video::IVideoDriver* Driver,
					IOSOperator* op)
{
	return new CGUIEnvironment(fs, Driver, op);
}


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_


// This file is part of VSTGUI. It is subject to the license terms
// in the LICENSE file found in the top-level directory of this
// distribution and at http://github.com/steinbergmedia/vstgui/LICENSE

#include "quartzgraphicspath.h"

#if MAC

#include "../iplatformgraphicspath.h"
#include "cfontmac.h"
#include "cgdrawcontext.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
class CGGraphicsPath : public IPlatformGraphicsPath
{
public:
	CGGraphicsPath (CGMutablePathRef path = nullptr);
	~CGGraphicsPath () noexcept;

	using PixelAlignPointFunc = CGPoint (*) (const CGPoint&, void* context);
	void pixelAlign (const PixelAlignPointFunc& func, void* context);

	CGPathRef getCGPathRef () const { return path; }

	// IPlatformGraphicsPath
	void addArc (const CRect& rect, double startAngle, double endAngle, bool clockwise) override;
	void addEllipse (const CRect& rect) override;
	void addRect (const CRect& rect) override;
	void addLine (const CPoint& to) override;
	void addBezierCurve (const CPoint& control1, const CPoint& control2,
	                     const CPoint& end) override;
	void beginSubpath (const CPoint& start) override;
	void closeSubpath () override;
	void finishBuilding () override;
	bool hitTest (const CPoint& p, bool evenOddFilled = false,
	              CGraphicsTransform* transform = nullptr) const override;
	CPoint getCurrentPosition () const override;
	CRect getBoundingBox () const override;

private:
	CGMutablePathRef path {nullptr};
};

//-----------------------------------------------------------------------------
static CGAffineTransform convert (const CGraphicsTransform& t)
{
	CGAffineTransform transform;
	transform.a = static_cast<CGFloat> (t.m11);
	transform.b = static_cast<CGFloat> (t.m21);
	transform.c = static_cast<CGFloat> (t.m12);
	transform.d = static_cast<CGFloat> (t.m22);
	transform.tx = static_cast<CGFloat> (t.dx);
	transform.ty = static_cast<CGFloat> (t.dy);
	return transform;
}

//-----------------------------------------------------------------------------
static CGMutablePathRef createTextPath (const CoreTextFont* font, UTF8StringPtr text)
{
	auto textPath = CGPathCreateMutable ();

	CFStringRef str = CFStringCreateWithCString (kCFAllocatorDefault, text, kCFStringEncodingUTF8);
	const void* keys[] = {kCTFontAttributeName};
	const void* values[] = {font->getFontRef ()};
	CFDictionaryRef dict =
	    CFDictionaryCreate (kCFAllocatorDefault, keys, values, 1,
	                        &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFAttributedStringRef attrString = CFAttributedStringCreate (kCFAllocatorDefault, str, dict);
	CFRelease (dict);
	CFRelease (str);

	CTLineRef line = CTLineCreateWithAttributedString (attrString);
	if (line != nullptr)
	{
		CCoord capHeight = font->getCapHeight ();
		CFArrayRef runArray = CTLineGetGlyphRuns (line);
		for (CFIndex runIndex = 0; runIndex < CFArrayGetCount (runArray); runIndex++)
		{
			CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex (runArray, runIndex);
			CTFontRef runFont =
			    (CTFontRef)CFDictionaryGetValue (CTRunGetAttributes (run), kCTFontAttributeName);
			CFIndex glyphCount = CTRunGetGlyphCount (run);
			for (CFRange glyphRange = CFRangeMake (0, 1); glyphRange.location < glyphCount;
			     ++glyphRange.location)
			{
				CGGlyph glyph;
				CGPoint position;
				CTRunGetGlyphs (run, glyphRange, &glyph);
				CTRunGetPositions (run, glyphRange, &position);
				CGPathRef letter = CTFontCreatePathForGlyph (runFont, glyph, nullptr);
				CGAffineTransform t = CGAffineTransformMakeTranslation (position.x, position.y);
				t = CGAffineTransformScale (t, 1, -1);
				t = CGAffineTransformTranslate (t, 0, static_cast<CGFloat> (-capHeight));
				CGPathAddPath (textPath, &t, letter);
				CGPathRelease (letter);
			}
		}
		CFRelease (line);
	}
	CFRelease (attrString);
	return textPath;
}

//-----------------------------------------------------------------------------
CGGraphicsPath::CGGraphicsPath (CGMutablePathRef inPath)
{
	if (inPath)
	{
		path = inPath;
		CFRetain (path);
	}
	else
	{
		path = CGPathCreateMutable ();
	}
}

//-----------------------------------------------------------------------------
CGGraphicsPath::~CGGraphicsPath () noexcept
{
	CFRelease (path);
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::addArc (const CRect& rect, double startAngle, double endAngle, bool clockwise)
{
	CCoord radiusX = (rect.right - rect.left) / 2.;
	CCoord radiusY = (rect.bottom - rect.top) / 2.;
	CGFloat centerX = static_cast<CGFloat> (rect.left + radiusX);
	CGFloat centerY = static_cast<CGFloat> (rect.top + radiusY);
	CGAffineTransform transform = CGAffineTransformMakeTranslation (centerX, centerY);
	transform = CGAffineTransformScale (transform, static_cast<CGFloat> (radiusX),
	                                    static_cast<CGFloat> (radiusY));
	startAngle = radians (startAngle);
	endAngle = radians (endAngle);
	if (radiusX != radiusY)
	{
		startAngle = std::atan2 (std::sin (startAngle) * radiusX, std::cos (startAngle) * radiusY);
		endAngle = std::atan2 (std::sin (endAngle) * radiusX, std::cos (endAngle) * radiusY);
	}
	if (CGPathIsEmpty (path))
	{
		CGPathMoveToPoint (path, &transform, static_cast<CGFloat> (std::cos (startAngle)),
		                   static_cast<CGFloat> (std::sin (startAngle)));
	}
	CGPathAddArc (path, &transform, 0, 0, 1, static_cast<CGFloat> (startAngle),
	              static_cast<CGFloat> (endAngle), !clockwise);
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::addEllipse (const CRect& rect)
{
	CGPathAddEllipseInRect (path, nullptr,
	                        CGRectMake (static_cast<CGFloat> (rect.left),
	                                    static_cast<CGFloat> (rect.top),
	                                    static_cast<CGFloat> (rect.getWidth ()),
	                                    static_cast<CGFloat> (rect.getHeight ())));
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::addRect (const CRect& rect)
{
	CGPathAddRect (path, nullptr,
	               CGRectMake (static_cast<CGFloat> (rect.left), static_cast<CGFloat> (rect.top),
	                           static_cast<CGFloat> (rect.getWidth ()),
	                           static_cast<CGFloat> (rect.getHeight ())));
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::addLine (const CPoint& to)
{
	CGPathAddLineToPoint (path, nullptr, static_cast<CGFloat> (to.x), static_cast<CGFloat> (to.y));
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::addBezierCurve (const CPoint& control1, const CPoint& control2,
                                     const CPoint& end)
{
	CGPathAddCurveToPoint (path, nullptr, static_cast<CGFloat> (control1.x),
	                       static_cast<CGFloat> (control1.y), static_cast<CGFloat> (control2.x),
	                       static_cast<CGFloat> (control2.y), static_cast<CGFloat> (end.x),
	                       static_cast<CGFloat> (end.y));
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::beginSubpath (const CPoint& start)
{
	CGPathMoveToPoint (path, nullptr, static_cast<CGFloat> (start.x),
	                   static_cast<CGFloat> (start.y));
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::closeSubpath ()
{
	CGPathCloseSubpath (path);
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::finishBuilding ()
{
}

//-----------------------------------------------------------------------------
void CGGraphicsPath::pixelAlign (const PixelAlignPointFunc& func, void* context)
{
	struct PathIterator
	{
		const PixelAlignPointFunc& pixelAlignFunc;
		void* context;
		CGMutablePathRef path;

		explicit PathIterator (const PixelAlignPointFunc& inPixelAlignFunc, void* inContext)
		: pixelAlignFunc (inPixelAlignFunc), context (inContext), path (CGPathCreateMutable ())
		{
		}
		void apply (const CGPathElement* element)
		{
			switch (element->type)
			{
				case kCGPathElementMoveToPoint:
				{
					element->points[0] = pixelAlignFunc (element->points[0], context);
					CGPathMoveToPoint (path, nullptr, element->points[0].x, element->points[0].y);
					break;
				}
				case kCGPathElementAddLineToPoint:
				{
					element->points[0] = pixelAlignFunc (element->points[0], context);
					CGPathAddLineToPoint (path, nullptr, element->points[0].x,
					                      element->points[0].y);
					break;
				}
				case kCGPathElementAddQuadCurveToPoint:
				{
					element->points[0] = pixelAlignFunc (element->points[0], context);
					element->points[1] = pixelAlignFunc (element->points[1], context);
					CGPathAddQuadCurveToPoint (path, nullptr, element->points[0].x,
					                           element->points[0].y, element->points[1].x,
					                           element->points[1].y);
					break;
				}
				case kCGPathElementAddCurveToPoint:
				{
					element->points[0] = pixelAlignFunc (element->points[0], context);
					element->points[1] = pixelAlignFunc (element->points[1], context);
					element->points[2] = pixelAlignFunc (element->points[2], context);
					CGPathAddCurveToPoint (path, nullptr, element->points[0].x,
					                       element->points[0].y, element->points[1].x,
					                       element->points[1].y, element->points[2].x,
					                       element->points[2].y);
					break;
				}
				case kCGPathElementCloseSubpath:
				{
					CGPathCloseSubpath (path);
					break;
				}
			}
		}

		static void apply (void* info, const CGPathElement* element)
		{
			PathIterator* This = static_cast<PathIterator*> (info);
			This->apply (element);
		}
	};
	PathIterator iterator (func, context);
	CGPathApply (path, &iterator, PathIterator::apply);
	CFRelease (path);
	path = iterator.path;
}

//-----------------------------------------------------------------------------
bool CGGraphicsPath::hitTest (const CPoint& p, bool evenOddFilled,
                              CGraphicsTransform* transform) const
{
	auto cgPoint = CGPointFromCPoint (p);
	CGAffineTransform cgTransform;
	if (transform)
		cgTransform = convert (*transform);
	return CGPathContainsPoint (path, transform ? &cgTransform : nullptr, cgPoint, evenOddFilled);
}

//-----------------------------------------------------------------------------
CPoint CGGraphicsPath::getCurrentPosition () const
{
	CPoint p (0, 0);
	if (!CGPathIsEmpty (path))
	{
		auto cgPoint = CGPathGetCurrentPoint (path);
		p = CPointFromCGPoint (cgPoint);
	}
	return p;
}

//-----------------------------------------------------------------------------
CRect CGGraphicsPath::getBoundingBox () const
{
	auto cgRect = CGPathGetBoundingBox (path);
	return CRectFromCGRect (cgRect);
}

//-----------------------------------------------------------------------------
CGAffineTransform QuartzGraphicsPath::createCGAffineTransform (const CGraphicsTransform& t)
{
	return convert (t);
}

//-----------------------------------------------------------------------------
QuartzGraphicsPath::QuartzGraphicsPath ()
{
}

//-----------------------------------------------------------------------------
QuartzGraphicsPath::QuartzGraphicsPath (const CoreTextFont* font, UTF8StringPtr text)

{
	auto textPath = createTextPath (font, text);
	path = std::make_unique<CGGraphicsPath> (textPath);
}

//-----------------------------------------------------------------------------
QuartzGraphicsPath::~QuartzGraphicsPath () noexcept
{
}

//-----------------------------------------------------------------------------
CGradient* QuartzGraphicsPath::createGradient (double color1Start, double color2Start,
                                               const CColor& color1, const CColor& color2)
{
	return new QuartzGradient (color1Start, color2Start, color1, color2);
}

//-----------------------------------------------------------------------------
void QuartzGraphicsPath::makeCGGraphicsPath ()
{
	path = std::make_unique<CGGraphicsPath> ();
	for (const auto& e : elements)
	{
		switch (e.type)
		{
			case Element::kArc:
			{
				path->addArc (rect2CRect (e.instruction.arc.rect), e.instruction.arc.startAngle,
				              e.instruction.arc.endAngle, e.instruction.arc.clockwise);
				break;
			}
			case Element::kEllipse:
			{
				path->addEllipse (rect2CRect (e.instruction.rect));
				break;
			}
			case Element::kRect:
			{
				path->addRect (rect2CRect (e.instruction.rect));
				break;
			}
			case Element::kLine:
			{
				path->addLine (point2CPoint (e.instruction.point));
				break;
			}
			case Element::kBezierCurve:
			{
				path->addBezierCurve (point2CPoint (e.instruction.curve.control1),
				                      point2CPoint (e.instruction.curve.control2),
				                      point2CPoint (e.instruction.curve.end));
				break;
			}
			case Element::kBeginSubpath:
			{
				path->beginSubpath (point2CPoint (e.instruction.point));
				break;
			}
			case Element::kCloseSubpath:
			{
				path->closeSubpath ();
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
bool QuartzGraphicsPath::ensurePathValid ()
{
	if (path == nullptr)
	{
		makeCGGraphicsPath ();
	}
	return path != nullptr;
}

//-----------------------------------------------------------------------------
CGPathRef QuartzGraphicsPath::getCGPathRef ()
{
	ensurePathValid ();
	return path->getCGPathRef ();
}

//-----------------------------------------------------------------------------
void QuartzGraphicsPath::dirty ()
{
	path = nullptr;
}

//-----------------------------------------------------------------------------
bool QuartzGraphicsPath::hitTest (const CPoint& p, bool evenOddFilled,
                                  CGraphicsTransform* transform)
{
	ensurePathValid ();
	return path->hitTest (p, evenOddFilled, transform);
}

//-----------------------------------------------------------------------------
CPoint QuartzGraphicsPath::getCurrentPosition ()
{
	ensurePathValid ();
	return path->getCurrentPosition ();
}

//-----------------------------------------------------------------------------
CRect QuartzGraphicsPath::getBoundingBox ()
{
	ensurePathValid ();
	return path->getBoundingBox ();
}

//-----------------------------------------------------------------------------
void QuartzGraphicsPath::pixelAlign (CDrawContext* context)
{
	CGDrawContext* cgDrawContext = dynamic_cast<CGDrawContext*> (context);
	if (cgDrawContext == nullptr)
		return;

	ensurePathValid ();

	path->pixelAlign (
	    [] (const CGPoint& p, void* context) {
		    auto cgDrawContext = reinterpret_cast<CGDrawContext*> (context);
		    return cgDrawContext->pixelAlligned (p);
	    },
	    cgDrawContext);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
QuartzGradient::QuartzGradient (const ColorStopMap& map) : CGradient (map), gradient (nullptr)
{
}

//-----------------------------------------------------------------------------
QuartzGradient::QuartzGradient (double _color1Start, double _color2Start, const CColor& _color1,
                                const CColor& _color2)
: CGradient (_color1Start, _color2Start, _color1, _color2), gradient (nullptr)
{
}

//-----------------------------------------------------------------------------
QuartzGradient::~QuartzGradient () noexcept
{
	releaseCGGradient ();
}

//-----------------------------------------------------------------------------
void QuartzGradient::addColorStop (const std::pair<double, CColor>& colorStop)
{
	CGradient::addColorStop (colorStop);
	releaseCGGradient ();
}

//-----------------------------------------------------------------------------
void QuartzGradient::addColorStop (std::pair<double, CColor>&& colorStop)
{
	CGradient::addColorStop (colorStop);
	releaseCGGradient ();
}

//-----------------------------------------------------------------------------
void QuartzGradient::createCGGradient () const
{
	CGFloat* locations = new CGFloat[colorStops.size ()];
	CFMutableArrayRef colors = CFArrayCreateMutable (
	    kCFAllocatorDefault, static_cast<CFIndex> (colorStops.size ()), &kCFTypeArrayCallBacks);

	uint32_t index = 0;
	for (const auto& it : colorStops)
	{
		locations[index] = static_cast<CGFloat> (it.first);
		CColor color = it.second;
		CFArrayAppendValue (colors, getCGColor (color));
		++index;
	}

	gradient = CGGradientCreateWithColors (GetCGColorSpace (), colors, locations);

	CFRelease (colors);
	delete[] locations;
}

//-----------------------------------------------------------------------------
void QuartzGradient::releaseCGGradient ()
{
	if (gradient)
	{
		CFRelease (gradient);
		gradient = nullptr;
	}
}

//-----------------------------------------------------------------------------
QuartzGradient::operator CGGradientRef () const
{
	if (gradient == nullptr)
	{
		createCGGradient ();
	}
	return gradient;
}

//-----------------------------------------------------------------------------
CGradient* CGradient::create (const ColorStopMap& colorStopMap)
{
	return new QuartzGradient (colorStopMap);
}

} // VSTGUI

#endif

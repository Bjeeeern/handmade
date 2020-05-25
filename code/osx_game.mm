#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

@interface OSX_ApplicationDelegate : NSObject<NSApplicationDelegate>
@end

@implementation OSX_ApplicationDelegate

- (BOOL)
	applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	NSLog(@"This is reached even though immediate termination is set. Only when"
				"the window is manually closed.");

	return YES;
}

- (NSApplicationTerminateReply)
	applicationShouldTerminate:(NSApplication *)sender
{ 
	NSLog(@"This is never reached due to immediate termination being set.");

	return NSTerminateNow;
}

- (void)
	applicationWillTerminate:(NSNotification *)aNotification
{
	NSLog(@"This is never reached due to immediate termination being set.");
}

- (void)
	applicationDidFinishLaunching:(NSNotification *)aNotification
{

	s32 pitch = WINDOW_WIDTH * sizeof(u32);
	u8 *buffer = malloc(pitch * WINDOW_HEIGHT);

	for (s32 y = 0; 
			 y < WINDOW_HEIGHT; 
			 ++y) 
	{
		u32 *row = (u32 *)(buffer + y * pitch);

		for (s32 x = 0; 
				 x < WINDOW_WIDTH; 
				 ++x) 
		{
			row[x] = 0xFF0000FF;
		}
	}

	NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] 
		initWithBitmapDataPlanes:&buffer
		pixelsWide:WINDOW_WIDTH pixelsHigh:WINDOW_HEIGHT
		bitsPerSample:8 
		samplesPerPixel:4 
		hasAlpha:YES 
		isPlanar:NO
		colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:pitch 
		bitsPerPixel:sizeof(s32) * 8];

	size_t window_width = self.window.contentView.bounds.size.width;
	size_t window_height = self.window.contentView.bounds.size.height;

	NSImage *image = [[NSImage alloc] 
		initWithSize:NSMakeSize(window_width, window_height)];
	[image addRepresentation:rep];

	self.window.contentView.wantsLayer = YES;
	self.window.contentView.layer.contents = image;
}

@end

int main(int argc, char *argv[])
{
	NSAutoreleasePool * auto_release_pool = [[NSAutoreleasePool alloc] init];

	//TODO(bjorn): Handle termination properly when writing saved data.
	[[NSProcessInfo processInfo] enableSuddenTermination];
	[NSApplication sharedApplication];

	OSX_ApplicationDelegate* app_delegate = [[OSX_ApplicationDelegate alloc] init];

	[NSApp setDelegate:[[OSX_ApplicationDelegate alloc] init]];

	NSWindow* window = [[[NSWindow alloc] 
		initWithContentRect:NSMakeRect(200, 200, WINDOW_WIDTH, WINDOW_HEIGHT)
		styleMask: NSWindowStyleMaskTitled|
		NSWindowStyleMaskFullScreen|
		NSWindowStyleMaskClosable|
		NSWindowStyleMaskMiniaturizable|
		NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO] autorelease];

	[window setBackgroundColor:[NSColor blueColor]];
	[window setTitle:@"A Game"];
	[window makeKeyAndOrderFront:nil];

	[NSApp run];

	return 0;
}

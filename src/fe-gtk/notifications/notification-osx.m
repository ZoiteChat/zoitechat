/* Native macOS notifications; GPL-2.0-or-later, see COPYING. */
#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>

@interface ZoiteChatNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end
@implementation ZoiteChatNotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
      willPresentNotification:(UNNotification *)notification
        withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    completionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionSound);
}
@end

static ZoiteChatNotificationDelegate *notification_delegate;

int
notification_backend_supported (void)
{
    return [[NSBundle mainBundle] bundleIdentifier] != nil;
}

int
notification_backend_init (const char **error)
{
    if (!notification_backend_supported ())
    {
        *error = "macOS notifications require an application bundle";
        return 0;
    }
    notification_delegate = [ZoiteChatNotificationDelegate new];
    [UNUserNotificationCenter currentNotificationCenter].delegate = notification_delegate;
    return 1;
}

void
notification_backend_show (const char *title, const char *text)
{
    UNMutableNotificationContent *content = [UNMutableNotificationContent new];
    content.title = [NSString stringWithUTF8String:title] ?: @"ZoiteChat";
    content.body = [NSString stringWithUTF8String:text] ?: @"";
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
    /* Ask only when the user first needs a notification, without blocking GTK. */
    [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
        completionHandler:^(BOOL granted, NSError *error) {
            if (granted)
            {
                UNNotificationRequest *request = [UNNotificationRequest
                    requestWithIdentifier:[[NSUUID UUID] UUIDString] content:content trigger:nil];
                [center addNotificationRequest:request withCompletionHandler:^(NSError *deliveryError) {
                    if (deliveryError)
                        NSLog(@"ZoiteChat notification failed: %@", deliveryError);
                }];
            }
            else if (error)
                NSLog(@"ZoiteChat notification authorization failed: %@", error);
        }];
}

void
notification_backend_deinit (void)
{
    if (notification_delegate)
        [UNUserNotificationCenter currentNotificationCenter].delegate = nil;
    notification_delegate = nil;
}

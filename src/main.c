#define GLFW_EXPOSE_NATIVE_EGL
#define XR_USE_PLATFORM_EGL
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_EXTENSION_PROTOTYPES

#include <EGL/egl.h>

#include <glad/gl.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int application_running = 1;
static int session_running = 0;
static XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;

static void setViewport(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

static void check_xr(XrResult result, const char *what) {
  if (XR_FAILED(result)) {
    fprintf(stderr, "%s failed: %d\n", what, result);
  }
}

void create_window(GLFWwindow **window) {
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
  glfwWindowHint(GLFW_SAMPLES, 8);

  *window = glfwCreateWindow(800, 600, "TheSeed", NULL, NULL);

  if (!window) {
    printf("Failed to create window");
  }

  glfwMakeContextCurrent(*window);

  if (!gladLoadGL(glfwGetProcAddress)) {
    printf("Failed to initialize GLAD");
    return;
  }

  setViewport(*window, 800, 600);
  glfwSetFramebufferSizeCallback(*window, setViewport);

  // Load OpenGL Extensions
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
}

void destroy_window(GLFWwindow **window) {
  glfwDestroyWindow(*window);
  *window = NULL;
}

XrInstance create_instance() {
  // Building the appinfo and the create info structs to create the instance
  // struct.
  XrInstance instance = XR_NULL_HANDLE;

  XrApplicationInfo app_info;
  memset(&app_info, 0, sizeof(app_info));
  strncpy(app_info.applicationName, "Simple OpenXR C App",
          XR_MAX_APPLICATION_NAME_SIZE - 1);
  strncpy(app_info.engineName, "TheSeed", XR_MAX_ENGINE_NAME_SIZE - 1);
  app_info.applicationVersion = 1;
  app_info.engineVersion = 1;
  app_info.apiVersion = XR_CURRENT_API_VERSION;

  const char *extensions[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
                              XR_MNDX_EGL_ENABLE_EXTENSION_NAME};

  XrInstanceCreateInfo create_info;
  memset(&create_info, 0, sizeof(create_info));
  create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
  create_info.next = NULL;
  create_info.createFlags = 0;
  create_info.applicationInfo = app_info;
  create_info.enabledApiLayerCount = 0;
  create_info.enabledApiLayerNames = NULL;
  create_info.enabledExtensionCount = 2;
  create_info.enabledExtensionNames = extensions;

  check_xr(xrCreateInstance(&create_info, &instance), "xrCreateInstance");
  return instance;
}

void destroy_instance(XrInstance instance) {
  check_xr(xrDestroyInstance(instance), "Failed to destroy Instance.");
}

void print_instance_properties(XrInstance instance) {
  XrInstanceProperties props;
  memset(&props, 0, sizeof(props));
  props.type = XR_TYPE_INSTANCE_PROPERTIES;

  if (XR_SUCCEEDED(xrGetInstanceProperties(instance, &props))) {
    printf("Runtime: %s\n", props.runtimeName);
    printf("Runtime version: %d.%d.%d\n",
           XR_VERSION_MAJOR(props.runtimeVersion),
           XR_VERSION_MINOR(props.runtimeVersion),
           XR_VERSION_PATCH(props.runtimeVersion));
  }
}

XrSystemId get_system_id(XrInstance instance, XrSystemProperties *out_props) {
  XrSystemId system_id = XR_NULL_SYSTEM_ID;

  XrSystemGetInfo system_info;
  memset(&system_info, 0, sizeof(system_info));
  system_info.type = XR_TYPE_SYSTEM_GET_INFO;
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  check_xr(xrGetSystem(instance, &system_info, &system_id), "xrGetSystem");

  if (out_props) {
    memset(out_props, 0, sizeof(*out_props));
    out_props->type = XR_TYPE_SYSTEM_PROPERTIES;
    check_xr(xrGetSystemProperties(instance, system_id, out_props),
             "xrGetSystemProperties");
  }

  return system_id;
}

void print_system_info(XrInstance instance) {
  XrSystemProperties props;
  XrSystemId system_id = get_system_id(instance, &props);

  if (system_id != XR_NULL_SYSTEM_ID) {
    printf("System ID acquired.\n");
    printf("System name: %s\n", props.systemName);
    printf("Vendor ID: %u\n", props.vendorId);
    printf("Max swapchain width: %u\n",
           props.graphicsProperties.maxSwapchainImageWidth);
    printf("Max swapchain height: %u\n",
           props.graphicsProperties.maxSwapchainImageHeight);
    printf("Position tracking: %s\n",
           props.trackingProperties.positionTracking ? "yes" : "no");
    printf("Orientation tracking: %s\n",
           props.trackingProperties.orientationTracking ? "yes" : "no");
  }
}

XrGraphicsBindingEGLMNDX create_opengl_binding(GLFWwindow *window) {
  XrGraphicsBindingEGLMNDX binding = {0};

  binding.type = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX;
  binding.next = NULL;

  binding.display = glfwGetEGLDisplay();
  binding.context = glfwGetEGLContext(window);

  binding.getProcAddress = (PFN_xrEglGetProcAddressMNDX)eglGetProcAddress;

  EGLint config_id;
  eglQueryContext(binding.display, binding.context, EGL_CONFIG_ID, &config_id);

  EGLint num_configs;
  EGLint attribs[] = {EGL_CONFIG_ID, config_id, EGL_NONE};
  eglChooseConfig(binding.display, attribs, &binding.config, 1, &num_configs);

  return binding;
}

void check_graphics_requirements(XrInstance instance, XrSystemId system_id) {
  // Step 1: Declare function pointer (type already defined in headers)
  PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetRequirements = NULL;

  // Step 2: Load the function dynamically
  XrResult result =
      xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                            (PFN_xrVoidFunction *)&pfnGetRequirements);

  if (XR_FAILED(result) || pfnGetRequirements == NULL) {
    fprintf(stderr, "Failed to load xrGetOpenGLGraphicsRequirementsKHR\n");
    exit(1);
  }

  // Step 3: Call the function through pointer
  XrGraphicsRequirementsOpenGLKHR requirements = {0};
  requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;

  check_xr(pfnGetRequirements(instance, system_id, &requirements),
           "xrGetOpenGLGraphicsRequirementsKHR");
}

XrSession create_session(XrInstance instance, XrSystemId system_id,
                         const XrGraphicsBindingEGLMNDX *gl_binding) {
  XrSession session = XR_NULL_HANDLE;

  XrSessionCreateInfo session_info;
  memset(&session_info, 0, sizeof(session_info));
  session_info.type = XR_TYPE_SESSION_CREATE_INFO;
  session_info.next =
      (const void *)gl_binding; /* must point to your OpenGL binding */
  session_info.createFlags = 0;
  session_info.systemId = system_id;

  check_xr(xrCreateSession(instance, &session_info, &session),
           "xrCreateSession");
  return session;
}

void destroy_session(XrSession session) { xrDestroySession(session); }

void poll_xr_events(XrInstance instance, XrSession session) {
  XrEventDataBuffer event_buffer;
  memset(&event_buffer, 0, sizeof(event_buffer));
  event_buffer.type = XR_TYPE_EVENT_DATA_BUFFER;

  while (xrPollEvent(instance, &event_buffer) == XR_SUCCESS) {
    switch (event_buffer.type) {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
      XrEventDataEventsLost *ev = (XrEventDataEventsLost *)&event_buffer;
      printf("OpenXR: lost %u events\n", ev->lostEventCount);
      break;
    }

    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
      printf("OpenXR: instance loss pending, shutting down\n");
      application_running = 0;
      break;
    }

    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      XrEventDataSessionStateChanged *ev =
          (XrEventDataSessionStateChanged *)&event_buffer;

      if (ev->session != session) {
        break;
      }

      session_state = ev->state;
      printf("OpenXR: session state changed to %d\n", session_state);

      if (session_state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo begin_info;
        memset(&begin_info, 0, sizeof(begin_info));
        begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
        begin_info.primaryViewConfigurationType =
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

        check_xr(xrBeginSession(session, &begin_info), "xrBeginSession");
        session_running = 1;
      } else if (session_state == XR_SESSION_STATE_STOPPING) {
        check_xr(xrEndSession(session), "xrEndSession");
        session_running = 0;
      } else if (session_state == XR_SESSION_STATE_EXITING) {
        printf("OpenXR: runtime requested exit\n");
        application_running = 0;
      } else if (session_state == XR_SESSION_STATE_LOSS_PENDING) {
        printf("OpenXR: session loss pending\n");
        application_running = 0;
      }

      break;
    }

    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      printf("OpenXR: interaction profile changed\n");
      break;

    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      printf("OpenXR: reference space change pending\n");
      break;

    default:
      printf("OpenXR: unhandled event type %d\n", event_buffer.type);
      break;
    }

    memset(&event_buffer, 0, sizeof(event_buffer));
    event_buffer.type = XR_TYPE_EVENT_DATA_BUFFER;
  }
}

int main() {
  // Initialize GLFW and OpenGL
  GLFWwindow *window;
  create_window(&window);

  // Initialize OpenXR
  XrInstance instance = create_instance();

  print_instance_properties(instance);
  print_system_info(instance);

  XrSystemId system_id = get_system_id(instance, NULL);

  check_graphics_requirements(instance, system_id);

  XrGraphicsBindingEGLMNDX binding = create_opengl_binding(window);

  XrSession session = create_session(instance, system_id, &binding);
  printf("Seesion Created!\n");

  while (application_running) {
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) {
      application_running = 0;
    }
    poll_xr_events(instance, session);
    if (session_running) {
      glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glfwSwapBuffers(window);
    }
  }

  printf("Destroying!\n");
  destroy_session(session);
  destroy_instance(instance);
  destroy_window(&window);
  glfwTerminate();

  return 0;
}

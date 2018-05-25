/* ****************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include "oc_api.h"
#include "port/oc_clock.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include <easysetup.h>

static pthread_mutex_t mutex;
static pthread_cond_t cv;
static struct timespec ts;
static int quit = 0;

static double temp_C = 5.0, min_C = 0.0, max_C = 100.0, min_K = 273.15,
              max_K = 373.15, min_F = 32, max_F = 212;
typedef enum { C = 100, F, K } units_t;

/**
 * @var gIsSecured
 * @brief Variable to check if secure mode is enabled or not.
 */
static bool gIsSecured = true;

static void ReadUserdataCb(oc_rep_t* payload, char* resourceType, void** userdata)
{
    (void)resourceType;
    (void)payload;
    (void)userdata;

    printf("[ES App] ReadUserdataCb IN");

    printf("[ES App] ReadUserdataCb OUT");
}

static void WriteUserdataCb(oc_rep_t* payload, char* resourceType)
{
    (void)resourceType;
    (void)payload;

    printf("[ES App] WriteUserdataCb IN");

    printf("[ES App] WriteUserdataCb OUT");
}

static int
app_init(void)
{
  int err = oc_init_platform("Samsung", NULL, NULL);

  // err |= oc_add_device("/oic/d", "oic.d.airconditioner", "[Floor A/C] Samsung", "ocf.1.0.0",
  //                      "ocf.res.1.0.0", NULL, NULL);

  err |= oc_add_device("/oic/d", "oic.d.washer", "[washer] Samsung", "ocf.1.0.0",
                       "ocf.res.1.0.0", NULL, NULL);
  return err;
}

static void
get_temp(oc_request_t *request, oc_interface_mask_t interface, void *user_data)
{
  (void)user_data;
  PRINT("[ES App] GET_temp:\n");
  bool invalid_query = false;
  double temp = temp_C;
  units_t temp_units = C;
  char *units;
  int units_len = oc_get_query_value(request, "units", &units);
  if (units_len != -1) {
    if (units[0] == 'K') {
      temp = temp_C + 273.15;
      temp_units = K;
    } else if (units[0] == 'F') {
      temp = (temp_C / 100) * 180 + 32;
      temp_units = F;
    } else if (units[0] != 'C')
      invalid_query = true;
  }

  oc_rep_start_root_object();
  switch (interface) {
  case OC_IF_BASELINE:
    oc_process_baseline_interface(request->resource);
    oc_rep_set_text_string(root, id, "home_thermostat");
  /* fall through */
  case OC_IF_A:
  case OC_IF_S:
    oc_rep_set_double(root, temperature, temp);
    switch (temp_units) {
    case C:
      oc_rep_set_text_string(root, units, "C");
      break;
    case F:
      oc_rep_set_text_string(root, units, "F");
      break;
    case K:
      oc_rep_set_text_string(root, units, "K");
      break;
    }
    break;
  default:
    break;
  }

  if (!invalid_query) {
    oc_rep_set_array(root, range);
    switch (temp_units) {
    case C:
      oc_rep_add_double(range, min_C);
      oc_rep_add_double(range, max_C);
      break;
    case K:
      oc_rep_add_double(range, min_K);
      oc_rep_add_double(range, max_K);
      break;
    case F:
      oc_rep_add_double(range, min_F);
      oc_rep_add_double(range, max_F);
      break;
    }
    oc_rep_close_array(root, range);
  }

  oc_rep_end_root_object();

  if (invalid_query)
    oc_send_response(request, OC_STATUS_FORBIDDEN);
  else
    oc_send_response(request, OC_STATUS_OK);
}

static void
post_temp(oc_request_t *request, oc_interface_mask_t interface, void *user_data)
{
  (void)interface;
  (void)user_data;
  PRINT("[ES App] POST_temp:\n");
  bool out_of_range = false;
  double temp = -1;

  oc_rep_t *rep = request->request_payload;
  while (rep != NULL) {
    switch (rep->type) {
    case OC_REP_DOUBLE:
      temp = rep->value.double_p;
      break;
    default:
      break;
    }
    rep = rep->next;
  }

  if (temp < min_C || temp > max_C)
    out_of_range = true;

  temp_C = temp;

  oc_rep_start_root_object();
  oc_rep_set_text_string(root, id, "home_thermostat");
  oc_rep_set_double(root, temperature, temp_C);
  oc_rep_set_text_string(root, units, "C");
  oc_rep_set_array(root, range);
  oc_rep_add_double(range, min_C);
  oc_rep_add_double(range, max_C);
  oc_rep_close_array(root, range);
  oc_rep_end_root_object();

  if (out_of_range)
    oc_send_response(request, OC_STATUS_FORBIDDEN);
  else
    oc_send_response(request, OC_STATUS_CHANGED);
}

void WiFiProvCbInApp(es_wifi_conf_data* eventData)
{
    printf("[ES App] WiFiProvCbInApp IN\n");

    if(eventData == NULL)
    {
        printf("ESWiFiProvData is NULL\n");
        return ;
    }

    printf("SSID : %s\n", eventData->ssid);
    printf("Password : %s\n", eventData->pwd);
    printf("AuthType : %d\n", eventData->authtype);
    printf("EncType : %d\n", eventData->enctype);

    printf("[ES App] WiFiProvCbInApp OUT\n");
}

void DevConfProvCbInApp(es_dev_conf_data* eventData)
{
    printf("[ES App] DevConfProvCbInApp IN\n");

    if(eventData == NULL)
    {
        printf("[ES App] ESDevConfProvData is NULL\n");
        return ;
    }

    printf("[ES App] DevConfProvCbInApp OUT\n");
}

void CloudDataProvCbInApp(es_coap_cloud_conf_data* eventData)
{
    printf("[ES App] CloudDataProvCbInApp IN\n");

    if(eventData == NULL)
    {
        printf("ESCloudProvData is NULL\n");
        return ;
    }

    printf("AuthCode : %s\n", eventData->auth_code);
    printf("AuthProvider : %s\n", eventData->auth_provider);
    printf("CI Server : %s\n", eventData->ci_server);

    printf("[ES App] CloudDataProvCbInApp OUT\n");
}

es_provisioning_callbacks_s gCallbacks = {
    .wifi_prov_cb = &WiFiProvCbInApp,
    .dev_conf_prov_cb = &DevConfProvCbInApp,
    .cloud_data_prov_cb = &CloudDataProvCbInApp
};

void StartEasySetup(void)
{
    printf("[ES App] StartEasySetup IN\n");

    es_connect_type resourcemMask = ES_WIFICONF_RESOURCE | ES_COAPCLOUDCONF_RESOURCE | ES_DEVCONF_RESOURCE;
    if(es_init_enrollee(gIsSecured, resourcemMask, gCallbacks) != ES_OK)
    {
        printf("[ES App] Easy Setup Enrollee init error!!\n");
        return;
    }

    printf("[ES App] ESInitEnrollee Success\n");

    // Set callbacks for Vendor Specific Properties
    es_set_callback_for_userdata(&ReadUserdataCb, &WriteUserdataCb);
    printf("[ES App] StartEasySetup OUT\n");
}

void SetDeviceInfo(void)
{
    printf("[ES App] SetDeviceInfo IN\n");

    char *device_name = "TEST_DEVICE";

    es_device_property device_property ={{{WIFI_11G, WIFI_11N, WIFI_11AC, WiFi_EOF },WIFI_5G},{{0}}};

    oc_new_string(&device_property.DevConf.device_name, device_name, strlen(device_name));

    if (es_set_device_property(&device_property) == ES_ERROR)
        printf("[ES App] es_set_device_property error!\n");

    printf("[ES App] SetDeviceInfo OUT\n");
}

void StopEasySetup(void)
{
    printf("[ES App] StopEasySetup IN\n");

    if (es_terminate_enrollee() == ES_ERROR)
    {
        printf("ESTerminateEnrollee Failed!!\n");
        return;
    }

    printf("[ES App] StopEasySetup OUT\n");
}

static void
register_resources(void)
{
    printf("[ES App] register_resources IN\n");

    oc_resource_t *temp = oc_new_resource("tempsensor", "/temp", 1, 0);
    oc_resource_bind_resource_type(temp, "oic.r.temperature");
    oc_resource_bind_resource_interface(temp, OC_IF_A);
    oc_resource_bind_resource_interface(temp, OC_IF_S);
    oc_resource_set_default_interface(temp, OC_IF_A);
    oc_resource_set_discoverable(temp, true);
    oc_resource_set_periodic_observable(temp, 1);
    oc_resource_set_request_handler(temp, OC_GET, get_temp, NULL);
    oc_resource_set_request_handler(temp, OC_POST, post_temp, NULL);
    oc_add_resource(temp);

#ifdef OC_SECURITY
    gIsSecured = true;
#else
    gIsSecured = false;
#endif

    StartEasySetup();
    SetDeviceInfo();

    printf("[ES App] register_resources OUT\n");
}

static void
signal_event_loop(void)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);
}

static void
handle_signal(int signal)
{
  (void)signal;
  signal_event_loop();
  quit = 1;
}

int
easysetup_main(void)
{
  int init;

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cv, NULL);

  static const oc_handler_t handler = {.init = app_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources =
                                         register_resources };

  oc_clock_time_t next_event;

  oc_set_mtu_size(2048);
  oc_set_max_app_data_size(8192);

#ifdef OC_SECURITY
  oc_storage_config("/mnt/smart_home_server_linux_creds");
#endif /* OC_SECURITY */

  init = oc_main_init(&handler);
  if (init < 0)
    return init;

  while (quit != 1) {
    next_event = oc_main_poll();
    pthread_mutex_lock(&mutex);
    if (next_event == 0) {
      pthread_cond_wait(&cv, &mutex);
    } else {
      ts.tv_sec = (next_event / OC_CLOCK_SECOND);
      ts.tv_nsec = (next_event % OC_CLOCK_SECOND) * 1.e09 / OC_CLOCK_SECOND;
      pthread_cond_timedwait(&cv, &mutex, &ts);
    }
    pthread_mutex_unlock(&mutex);
  }

  oc_main_shutdown();

  StopEasySetup();

  printf("[ES App] Exit..\n");
  return 0;
}

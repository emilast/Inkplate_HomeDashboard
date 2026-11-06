// Host-side unit tests for src/PublicTransport/DeparturesParser.cpp. Run with: pio test -e native
#include <string.h>
#include <unity.h>

#include "PublicTransport/DeparturesParser.h"

static DepartureList departures;

void setUp()
{
    memset(&departures, 0, sizeof(departures));
}

void tearDown()
{
}

static void parse(const char *json, const char *transportType)
{
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    parseDepartures(doc, transportType, departures);
}

// ---- parseDepartures ----

void test_departure_fields()
{
    parse(R"({"departures":[{
        "destination":"Stuvsta","display":"3 min","direction_code":1,
        "scheduled":"2026-11-06T17:04:00","expected":"2026-11-06T17:06:00",
        "line":{"designation":"744","transport_mode":"BUS"},
        "journey":{"state":"EXPECTED"}}]})",
          "BUS");

    TEST_ASSERT_TRUE(departures.status == DepartureStatus::Ok);
    TEST_ASSERT_EQUAL_INT(1, departures.count);
    const Departure &d = departures.departures[0];
    TEST_ASSERT_EQUAL_STRING("744", d.line);
    TEST_ASSERT_EQUAL_STRING("Stuvsta", d.destination);
    TEST_ASSERT_EQUAL_STRING("3 min", d.displayTime);
    TEST_ASSERT_EQUAL_INT(17, d.scheduledHour);
    TEST_ASSERT_EQUAL_INT(4, d.scheduledMinute);
    TEST_ASSERT_EQUAL_INT(17, d.expectedHour);
    TEST_ASSERT_EQUAL_INT(6, d.expectedMinute);
    TEST_ASSERT_FALSE(d.cancelled);
    TEST_ASSERT_EQUAL_INT(0, d.deviationCount);
}

void test_departures_of_other_transport_modes_are_skipped()
{
    parse(R"({"departures":[
        {"destination":"A","line":{"transport_mode":"TRAIN"}},
        {"destination":"B","line":{"transport_mode":"BUS"}},
        {"destination":"C","line":{"transport_mode":"TRAIN"}}]})",
          "TRAIN");

    TEST_ASSERT_EQUAL_INT(2, departures.count);
    TEST_ASSERT_EQUAL_STRING("A", departures.departures[0].destination);
    TEST_ASSERT_EQUAL_STRING("C", departures.departures[1].destination);
}

void test_destination_is_converted_to_latin1()
{
    parse(R"({"departures":[{"destination":"Snättringe","line":{"transport_mode":"BUS"}}]})", "BUS");

    TEST_ASSERT_EQUAL_STRING("Sn\xe4ttringe", departures.departures[0].destination);
}

void test_missing_fields_get_defaults()
{
    parse(R"({"departures":[{"line":{"transport_mode":"BUS"}}]})", "BUS");

    const Departure &d = departures.departures[0];
    TEST_ASSERT_EQUAL_STRING("Ok\xe4nd destination", d.destination);
    TEST_ASSERT_EQUAL_STRING("", d.line);
    TEST_ASSERT_EQUAL_STRING("", d.displayTime);
    TEST_ASSERT_EQUAL_INT(-1, d.scheduledHour);
    TEST_ASSERT_EQUAL_INT(-1, d.expectedHour);
}

void test_cancelled_departure()
{
    parse(R"({"departures":[{"line":{"transport_mode":"BUS"},"journey":{"state":"CANCELLED"}}]})", "BUS");

    TEST_ASSERT_TRUE(departures.departures[0].cancelled);
}

void test_deviations_skip_empty_messages_and_are_capped()
{
    parse(R"({"departures":[{"line":{"transport_mode":"BUS"},"deviations":[
        {"message":"Försenad"},{"message":""},{"importance":5},
        {"message":"Two"},{"message":"Three"},{"message":"Four"}]}]})",
          "BUS");

    const Departure &d = departures.departures[0];
    TEST_ASSERT_EQUAL_INT(MAX_DEVIATIONS_PER_DEPARTURE, d.deviationCount);
    TEST_ASSERT_EQUAL_STRING("F\xf6rsenad", d.deviations[0]);
    TEST_ASSERT_EQUAL_STRING("Two", d.deviations[1]);
    TEST_ASSERT_EQUAL_STRING("Three", d.deviations[2]);
}

void test_departure_count_is_capped()
{
    JsonDocument doc;
    for (int i = 0; i < MAX_DEPARTURES + 3; i++)
        doc["departures"][i]["line"]["transport_mode"] = "BUS";
    parseDepartures(doc, "BUS", departures);

    TEST_ASSERT_EQUAL_INT(MAX_DEPARTURES, departures.count);
}

void test_missing_departures_array()
{
    departures.count = 5;
    departures.status = DepartureStatus::HttpError;
    parse("{}", "BUS");

    TEST_ASSERT_TRUE(departures.status == DepartureStatus::Ok);
    TEST_ASSERT_EQUAL_INT(0, departures.count);
}

// ---- parseErrorMessage ----

static void assertErrorMessage(const char *body, const char *expected)
{
    char message[256];
    parseErrorMessage(body, message, sizeof(message));
    TEST_ASSERT_EQUAL_STRING(expected, message);
}

void test_error_message_from_json_fields()
{
    assertErrorMessage(R"({"message":"Ogiltigt site-id"})", "Ogiltigt site-id");
    assertErrorMessage(R"({"error":"Not found"})", "Not found");
    assertErrorMessage(R"({"description":"Bad request"})", "Bad request");
    assertErrorMessage(R"({"message":"First","error":"Second"})", "First");
}

void test_error_message_falls_back_to_body()
{
    assertErrorMessage("Service Unavailable", "Service Unavailable");
    assertErrorMessage(R"({"status":500})", R"({"status":500})");
}

void test_error_message_for_empty_body()
{
    assertErrorMessage("", "(Tomt svar)");
    assertErrorMessage(nullptr, "(Tomt svar)");
}

void test_error_message_body_is_truncated()
{
    char body[400];
    memset(body, 'x', sizeof(body) - 1);
    body[sizeof(body) - 1] = '\0';

    char message[256];
    parseErrorMessage(body, message, sizeof(message));
    TEST_ASSERT_EQUAL_size_t(200, strlen(message));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_departure_fields);
    RUN_TEST(test_departures_of_other_transport_modes_are_skipped);
    RUN_TEST(test_destination_is_converted_to_latin1);
    RUN_TEST(test_missing_fields_get_defaults);
    RUN_TEST(test_cancelled_departure);
    RUN_TEST(test_deviations_skip_empty_messages_and_are_capped);
    RUN_TEST(test_departure_count_is_capped);
    RUN_TEST(test_missing_departures_array);
    RUN_TEST(test_error_message_from_json_fields);
    RUN_TEST(test_error_message_falls_back_to_body);
    RUN_TEST(test_error_message_for_empty_body);
    RUN_TEST(test_error_message_body_is_truncated);
    return UNITY_END();
}

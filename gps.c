//gcc gps.c -o gps -lcjson && ./gps && rm gps

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#define GPS_PATH_BASE "/opt/IMAGINE-B5G/realtime/GPS/"
#define SENSOR_GET_CMD "python3 /opt/utils/get_sensor_id.py > sensor.txt"
#define GPSPIPE_CMD "gpspipe -w -n 10 | grep -m 1 lon > temp.txt"
#define COL_NAME "sensor_id,date,lat,long,alt\n"
#define MAX_BUFFER_SIZE 1024




char* reformat_date(char* data, int jt){
    data[11]=' ';
    int i;
    if(jt==1){
        char* time=malloc(8);
        for(i=12; i< 20; i++){
            time[i-12]=data[i];
        }
        return time;
    }
    else{
        char* date=malloc(19);
        for(i=1; i<20; i++){
	    date[i-1]=data[i];
	    //printf("date[i]: %c ", date[i-1]);
        }
	printf("\n");
        return date;
    }
}

char* get_sensor_id() {
    char* line = malloc(5);  // Allocate memory for the sensor ID
    if (line == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }
    system(SENSOR_GET_CMD);
    FILE* file = fopen("sensor.txt", "r");
    if (file != NULL) {
        fgets(line, 5, file);
	//printf("line: %s\n", line);
        fclose(file); // Close the file after reading
    } else {
        fprintf(stderr, "Unable to open sensor.txt!\n");
        free(line);
        return NULL;
    }
    remove("sensor.txt");  // Clean up temporary file
    return line;           // Return allocated string
}


void write_row(char* src,char* dest,int start,int stop){
    for (int i=start; i<stop; i++){
        dest[i]=src[i-start];
    }
}

void write_data(const char* data, const char* hist_path, const char* rt_path) {
    if (!data) {
        fprintf(stderr, "No data.\n");
        return;
    }

    char row_rt[1024];
    char row_hist[1024];

    cJSON* data_json = cJSON_Parse(data);
    if (!data_json) {
        fprintf(stderr, "Failed to parse JSON data.\n");
        return;
    }

    FILE* fp_hist = fopen(hist_path, "a");
    FILE* fp_rt = fopen(rt_path, "w");
    if (!fp_hist || !fp_rt) {
        fprintf(stderr, "Failed to open files for writing.\n");
        cJSON_Delete(data_json);
        fclose(fp_hist);
        fclose(fp_rt);
        return;
    }

    // Write headers if history file is empty
    fseek(fp_hist, 0, SEEK_END);
    if (ftell(fp_hist) == 0) {
        fputs(COL_NAME, fp_hist);
    }
    fputs(COL_NAME, fp_rt);


    char* sensor_id = get_sensor_id();
    if (!sensor_id) {
        fclose(fp);
        fclose(fp_rt);
        cJSON_Delete(data_json);
        return;
    }

    int len = snprintf(row_hist, strlen(sensor_id), "%s,", sensor_id);
    int len_rt = snprintf(row_rt, strlen(sensor_id), "%s,", sensor_id);;

    const cJSON* date_j = cJSON_GetObjectItemCaseSensitive(data_json, "time");
    if (date_j) {
        char* unf_date = cJSON_PrintUnformatted(date_j);
        char* date = reformat_date(unf_date, 0);
        char* time = reformat_date(unf_date, 1);
        free(unf_date);

        if (date && time) {
            len += snprintf(row_hist, strlen(date), "%s,", date);
            len_rt += snprintf(row_rt, strlen(time), "%s,", time);
            free(date);
            free(time);
        }
    }

    const cJSON* lat_j = cJSON_GetObjectItemCaseSensitive(data_json, "lat");
    const cJSON* lon_j = cJSON_GetObjectItemCaseSensitive(data_json, "lon");
    const cJSON* alt_j = cJSON_GetObjectItemCaseSensitive(data_json, "alt");

    if (lat_j) {
        char* lat = cJSON_Print(lat_j);
        len += snprintf(row_hist, strlen(lat), "%s,", lat);
        len_rt += snprintf(row_rt, strlen(lat), "%s,", lat);
        free(lat);
    }
    if (lon_j) {
        char* lon = cJSON_Print(lon_j);
        len += snprintf(row_hist, strlen(lon), "%s,", lon);
        len_rt += snprintf(row_rt, strlen(lon), "%s,", lon);
        free(lon);
    }
    if (alt_j) {
        char* alt = cJSON_PrintUnformatted(alt_j);
        len += snprintf(row_hist, strlen(alt), "%s\n", alt);
        len_rt += snprintf(row_rt, strlen(alt), "%s\n", alt);
        free(alt);
    }
    printf("row hist: %s\n", row_hist);
    printf("row rt: %s\n", row_rt);

    fputs(row_hist, fp_hist);
    fputs(row_rt, fp_rt);

    free(sensor_id);
    cJSON_Delete(data_json);
    fclose(fp_hist);
    fclose(fp_rt);
}

void get_lat_lon_h(const char* hist_path, const char* rt_path) {
    FILE *fptr;
    system(GPSPIPE_CMD);
    fptr = fopen("temp.txt", "r");
    
    char buffer[2048];
    if (fptr != NULL) {
        while (fgets(buffer, sizeof(buffer), fptr)) {
            write_data(buffer, hist_path, rt_path);
        }
        fclose(fptr);
        remove("temp.txt");  // Remove the temporary file
    } else {
        fprintf(stderr, "Unable to open file!\n");
    }
}

int main() {
    char hist_path[MAX_BUFFER_SIZE], rt_path[MAX_BUFFER_SIZE];
    char date[11];
    time_t now = time(NULL);
    struct tm* tm_struct = localtime(&now);
    strftime(date, sizeof(date), "%Y-%m-%d", tm_struct);
    char* sensor_id = get_sensor_id();
    if (!sensor_id){
	    perror("Error getting sesnor value");
	    return 1;
    }
    snprintf(hist_path, sizeof(hist_path), "%s%s_gps_%s.csv", GPS_PATH_BASE, sensor_id, date);
    snprintf(rt_path, sizeof(rt_path), "%s%s_gps_realtime.csv", GPS_PATH_BASE, sensor_id);

    free(sensor_id);
    //printf("rt_path: %s\n", rt_path);
    //printf("hist_path: %s\n", hist_path);
    while (1) {
       	//get_lat_lon_h(hist_path, rt_path);
        sleep(10);  // Sleep for 10 seconds
    }
    return 0;
}

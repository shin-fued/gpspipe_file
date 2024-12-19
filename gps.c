//gcc gps.c -o gps -lcjson && ./gps && rm gps

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>

static const char* col_name="sensor_id,date,lat,long,alt\n";
static char gps_path[100] = "/opt/IMAGINE-B5G/realtime/GPS/";
static char gps_path_rt[100] = "/opt/IMAGINE-B5G/realtime/GPS/";


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
    system("python3 /opt/utils/get_sensor_id.py > sensor.txt");
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

char* strip_path(char* name){
	int n;
	while(name[n]<'\0'){n++;}
	char* path = malloc(n);
    for(int i=0; i<n; i++){
        path[i]=name[i];
    }
	return path;
}

void write_data(const char* data) {
    if (!data) return;

    char row_rt[1024] = {0};
    char row_hist[1024] = {0};

    cJSON* data_json = cJSON_Parse(data);
    if (!data_json) {
        fprintf(stderr, "Failed to parse JSON data.\n");
        return;
    }

    FILE* fp = fopen(gps_path, "a");
    if (!fp) {
        fprintf(stderr, "Failed to open history file.\n");
        cJSON_Delete(data_json);
        return;
    }

    FILE* fp_rt = fopen(gps_path_rt, "w");
    if (!fp_rt) {
        fprintf(stderr, "Failed to open realtime file.\n");
        fclose(fp);
        cJSON_Delete(data_json);
        return;
    }

    // Write headers if the history file is empty
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        fprintf(fp, "%s", col_name);
    }
    fprintf(fp_rt, "%s", col_name);

    char* sensor_id = get_sensor_id();
    if (!sensor_id) {
        fclose(fp);
        fclose(fp_rt);
        cJSON_Delete(data_json);
        return;
    }

    int len = snprintf(row_hist, sizeof(row_hist), "%s,", sensor_id);
    int len_rt = snprintf(row_rt, sizeof(row_rt), "%s,", sensor_id);;

    const cJSON* date_j = cJSON_GetObjectItemCaseSensitive(data_json, "time");
    if (date_j) {
        char* unf_date = cJSON_PrintUnformatted(date_j);
        char* date = reformat_date(unf_date, 0);
        char* time = reformat_date(unf_date, 1);
        free(unf_date);

        if (date && time) {
            len += snprintf(row_hist + len, sizeof(row_hist) - len, "%s,", date);
            len_rt += snprintf(row_rt + len_rt, sizeof(row_rt) - len_rt, "%s,", time);
            free(date);
            free(time);
        }
    }

    const cJSON* lat_j = cJSON_GetObjectItemCaseSensitive(data_json, "lat");
    const cJSON* lon_j = cJSON_GetObjectItemCaseSensitive(data_json, "lon");
    const cJSON* alt_j = cJSON_GetObjectItemCaseSensitive(data_json, "alt");

    if (lat_j) {
        char* lat = cJSON_PrintUnformatted(lat_j);
        len += snprintf(row_hist + len, sizeof(row_hist) - len, "%s,", lat);
        len_rt += snprintf(row_rt + len_rt, sizeof(row_rt) - len_rt, "%s,", lat);
        free(lat);
    }
    if (lon_j) {
        char* lon = cJSON_PrintUnformatted(lon_j);
        len += snprintf(row_hist + len, sizeof(row_hist) - len, "%s,", lon);
        len_rt += snprintf(row_rt + len_rt, sizeof(row_rt) - len_rt, "%s,", lon);
        free(lon);
    }
    if (alt_j) {
        char* alt = cJSON_PrintUnformatted(alt_j);
        len += snprintf(row_hist + len, sizeof(row_hist) - len, "%s\n", alt);
        len_rt += snprintf(row_rt + len_rt, sizeof(row_rt) - len_rt, "%s\n", alt);
        free(alt);
    }

    fputs(row_hist, fp);
    fputs(row_rt, fp_rt);

    free(sensor_id);
    cJSON_Delete(data_json);
    fclose(fp);
    fclose(fp_rt);
}

void get_lat_lon_h() {
    FILE *fptr;
    char* cmd = "gpspipe -w -n 10 | grep -m 1 lon > temp.txt";
    system(cmd);
    fptr = fopen("temp.txt", "r");
    
    char buffer[2048];
    if (fptr != NULL) {
        while (fgets(buffer, sizeof(buffer), fptr)) {
            write_data(buffer);
        }
        fclose(fptr);
        remove("temp.txt");  // Remove the temporary file
    } else {
        fprintf(stderr, "Unable to open file!\n");
    }
}

int main() {
    while (1) {
        char dt[11];  // Buffer to hold the date in "YYYY-MM-DD" format
        time_t now = time(NULL);  // Get the current time
        struct tm *tm_struct = localtime(&now);  // Convert to local time structure

        if (tm_struct != NULL) {
            // Format the date into the string
            strftime(dt, sizeof(dt), "%Y-%m-%d", tm_struct);
        } else {
            fprintf(stderr, "Failed to retrieve the current time.\n");
        }
        char* sensor=get_sensor_id();
	    if (!sensor) {
            break;
        }
	    char sensor_id[5];
        for(int i=0; i<5; i++){
            sensor_id[i]=sensor[i];
        }
        free(sensor);
	//printf("sensor: %s\n", sensor_id);
        strcat(gps_path, sensor_id);
        strcat(gps_path_rt, sensor_id);

        strcat(gps_path, "_gps_");
        strcat(gps_path_rt, "_gps_realtime.csv\0");
	printf("date: %s", dt);
        strcat(gps_path, dt);
        strcat(gps_path, ".csv\0");
        printf("gps_path: %s", gps_path);
	get_lat_lon_h();
        sleep(10);  // Sleep for 10 seconds
    }
    return 0;
}

from __future__ import print_function
import os.path
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
import argparse

# If modifying these scopes, delete the file token.json.
SCOPES = ['https://www.googleapis.com/auth/spreadsheets.readonly']

# The ID and range of a sample spreadsheet.
SPREADSHEET_ID = '1c6ZUrbWgOq66u53IjeeCCzyYUrCwcwBDkqT4Wepr6Os'
RANGE_NAME = 'Form Responses 1!A1:V'


def main():
    """Shows basic usage of the Sheets API.
    Prints values from a sample spreadsheet.
    """
    parser = argparse.ArgumentParser(
        description='Download google sheet into csv')
    parser.add_argument('--out', type=str, required=True,
                        help='output csv file name')
    args = parser.parse_args()

    creds = None
    # The file token.json stores the user's access and refresh tokens, and is
    # created automatically when the authorization flow completes for the first
    # time.
    if os.path.exists('token.json'):
        creds = Credentials.from_authorized_user_file('token.json', SCOPES)
    # If there are no (valid) credentials available, let the user log in.
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file(
                'credentials.json', SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for the next run. The file holds a live
        # refresh token, so create it owner-readable only (0600) instead
        # of relying on the process umask.
        fd = os.open('token.json',
                     os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(fd, 'w') as token:
            token.write(creds.to_json())
        # Tighten permissions of a token.json created by earlier runs
        # (O_CREAT mode only applies to newly created files).
        os.chmod('token.json', 0o600)

    try:
        service = build('sheets', 'v4', credentials=creds)

        # Call the Sheets API
        sheet = service.spreadsheets()
        result = sheet.values().get(spreadsheetId=SPREADSHEET_ID,
                                    range=RANGE_NAME).execute()
        values = result.get('values', [])

        if not values:
            print('No data found.')
            return

        import csv
        _CSV_FORMULA_LEAD = ('=', '+', '-', '@', '\t', '\r')

        def _csv_safe(v):
            s = str(v)
            return ("'" + s) if (s and s[0] in _CSV_FORMULA_LEAD) else s

        with open(args.out, 'w', encoding='utf-8', newline='') as csv_file:
            writer = csv.writer(csv_file)
            for row in values:
                writer.writerow([_csv_safe(cell) for cell in row])
    except HttpError as err:
        print(err)


if __name__ == '__main__':
    main()

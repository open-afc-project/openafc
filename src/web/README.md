# FaceBook AFC Web GUI

This a project containing the files required to develop and build web files to be hosted on a server. The project is running on a Typescript/React system built using Webpack and utilizes the Patternfly 4 library for styling. It also uses the Google Maps API for GeoJson display.

## Quick-start
Note that you will need to generate `.js` webpack files from the `.in` files using build.sh in the project root.
```bash
npm install yarn -g # ensure you have yarn on your machine globally
cd web # navigate into the web project directory
yarn install # install project dev dependencies
yarn build # build the project
yarn start # runs web GUI only on localhost
```

## Running in RAT context
Use the shell scripts in the root of the entire project to build and run
```bash
sh build.sh # build fbrat project
sh build-rpm.sh # build fbrat rpm
sh clean.sh # clean project
sh run.sh rat-manage-api runserver # start server
sh run.sh celery worker -A ratapi.tasks.afc_worker -E # start celery workers (note that rabbitmq queue must be running as daemon)
```

## Development Scripts

Install development/build dependencies
`yarn`

Start the development server
`yarn start`

Run a full build
`yarn build`

Run the test suite
`yarn test`

Run the linter
`yarn lint`

Generate these docs
`yarn build-docs`
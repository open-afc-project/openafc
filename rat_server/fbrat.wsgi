# Entry point for the RAT application
import ratapi

# Flask application
app = ratapi.create_app(
    config_override={
        'APPLICATION_ROOT': '/fbrat',
    }
)

# Expose WSGI
application = app.wsgi_app
